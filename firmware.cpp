// g++ firmware.cpp -o firmware -lpthread
// ./firmware
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <cstring>
#include "vgpu_common.h"

class VgpuDevice {
private:
    int fd;
    vgpu_shared_state* mem;

public:
    VgpuDevice(const char* path) {
        fd = open(path, O_RDWR);
        if (fd < 0) throw std::runtime_error("Failed to open /dev/vgpu0. Did you load the LKM?");
        mem = static_cast<vgpu_shared_state*>(
            mmap(nullptr, sizeof(vgpu_shared_state), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)
        );
        if (mem == MAP_FAILED) { close(fd); throw std::runtime_error("Mmap failed"); }
    }
    ~VgpuDevice() {
        if (mem != MAP_FAILED) munmap(mem, sizeof(vgpu_shared_state));
        if (fd >= 0) close(fd);
    }
    vgpu_shared_state* get_mem() const { return mem; }
    int get_fd() const { return fd; }
};

// --- 舊有邏輯：ARM64 組合語言 ---
uint32_t asm_add(uint32_t a, uint32_t b) {
    uint32_t res;
    __asm__ volatile ("add %w0, %w1, %w2" : "=r" (res) : "r" (a), "r" (b));
    return res;
}

// --- 舊有邏輯：Watchdog ---
void watchdog_thread(vgpu_shared_state* gpu) {
    std::cout << "[Watchdog] Monitoring thread started..." << std::endl;
    while (gpu->running) {
        uint64_t current_time = (uint64_t)time(NULL);
        if (current_time - gpu->last_heartbeat > 3) {
            std::cerr << "\n[!!! CRITICAL !!!] WATCHDOG TIMEOUT DETECTED! Hard Resetting..." << std::endl;
            gpu->watchdog_reset_count++;
            gpu->temperature = 37.0f;
            gpu->last_heartbeat = current_time; 
            memset(gpu->vram, 0, sizeof(gpu->vram)); // 清空 VRAM 以示重置
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void put_pixel(vgpu_shared_state* gpu, int x, int y, uint32_t color) {
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
        gpu->vram[y * WIDTH + x] = color;
    }
}

void process_command(vgpu_shared_state* gpu, vgpu_command& cmd) {
    switch (cmd.type) {
        case 1: { // CMD_CLEAR
            uint32_t color = cmd.params[0];
            for (int i = 0; i < WIDTH * HEIGHT; ++i) gpu->vram[i] = color;
            break;
        }
        case 2: { // CMD_DRAW_RECT
            int x = cmd.params[0], y = cmd.params[1], w = cmd.params[2], h = cmd.params[3];
            uint32_t color = cmd.params[4];
            for (int dy = 0; dy < h; ++dy)
                for (int dx = 0; dx < w; ++dx)
                    put_pixel(gpu, x + dx, y + dy, color);
            gpu->temperature += 0.8f;
            break;
        }
        case 4: { // CMD_CHECKSUM (ARM64 ASM)
            uint32_t res = asm_add(cmd.params[0], cmd.params[1]);
            std::cout << "[ASM Accelerator] Result = " << res << std::endl;
            break;
        }
        case 9: { // CMD_HANG
            std::cout << "[Firmware] Received HANG. Freezing..." << std::endl;
            while(true) { usleep(100); } // 故意卡死，觸發 Watchdog
            break;
        }
    }
}

int main() {
    try {
        VgpuDevice dev("/dev/vgpu0");
        vgpu_shared_state* gpu = dev.get_mem();

        // 註冊 eventfd 中斷
        int irq_fd = eventfd(0, EFD_NONBLOCK);
        if (ioctl(dev.get_fd(), VGPU_IOCTL_SET_EVENTFD, irq_fd) < 0) {
            throw std::runtime_error("Failed to register IRQ");
        }

        // 初始化遙測數據
        gpu->temperature = 37.0f;
        gpu->last_heartbeat = (uint64_t)time(NULL);

        // 啟動 Watchdog (改用 C++11 std::thread)
        std::thread wd_thread(watchdog_thread, gpu);
        wd_thread.detach();

        std::cout << "[vGPU Firmware] Booted. Waiting for Kernel IRQ..." << std::endl;

        std::atomic<uint32_t>* head = reinterpret_cast<std::atomic<uint32_t>*>(&gpu->head);
        std::atomic<uint32_t>* tail = reinterpret_cast<std::atomic<uint32_t>*>(&gpu->tail);
        uint64_t irq_count;

        while (gpu->running) {
            gpu->last_heartbeat = (uint64_t)time(NULL); // 餵狗
            gpu->frame_counter++;

            // 溫度降頻與冷卻邏輯
            if (gpu->temperature > 85.0f) {
                std::cout << "[Thermal] Overheating! Throttling..." << std::endl;
                usleep(50000); // 降頻
                gpu->temperature -= 0.5f;
            } else if (gpu->temperature > 37.0f) {
                gpu->temperature -= 0.01f; // 自然冷卻
            }

            // 阻塞等待中斷 (Polling eventfd，使用 select 或直接 non-block read)
            if (read(irq_fd, &irq_count, sizeof(irq_count)) > 0) {
                // 收到 Kernel 通知，處理 Ring Buffer
                uint32_t current_tail = tail->load(std::memory_order_acquire);
                uint32_t current_head = head->load(std::memory_order_relaxed);

                while (current_head != current_tail) {
                    vgpu_command cmd = gpu->ring[current_head];
                    process_command(gpu, cmd);
                    current_head = (current_head + 1) % RING_BUFFER_SIZE;
                }
                
                head->store(current_head, std::memory_order_release);
            } else {
                usleep(1000); // Idle 狀態
            }
        }
        close(irq_fd);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}