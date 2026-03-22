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
#include "vgpu_common.h"

// RAII 封裝 File Descriptor 與 Mmap 資源
class VgpuDevice {
private:
    int fd;
    vgpu_shared_state* mem;

public:
    VgpuDevice(const char* path) {
        fd = open(path, O_RDWR);
        if (fd < 0) throw std::runtime_error("Failed to open device");

        mem = static_cast<vgpu_shared_state*>(
            mmap(nullptr, sizeof(vgpu_shared_state), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)
        );
        if (mem == MAP_FAILED) {
            close(fd);
            throw std::runtime_error("Mmap failed");
        }
    }

    ~VgpuDevice() {
        if (mem != MAP_FAILED) munmap(mem, sizeof(vgpu_shared_state));
        if (fd >= 0) close(fd);
    }

    vgpu_shared_state* get_mem() const { return mem; }
    int get_fd() const { return fd; }
};

int main() {
    try {
        VgpuDevice dev("/dev/vgpu0");
        vgpu_shared_state* gpu = dev.get_mem();

        // 建立 eventfd 作為硬體中斷線 (IRQ)
        int irq_fd = eventfd(0, EFD_NONBLOCK);
        if (ioctl(dev.get_fd(), VGPU_IOCTL_SET_EVENTFD, irq_fd) < 0) {
            throw std::runtime_error("Failed to register IRQ");
        }

        std::cout << "[Firmware] Booted. Waiting for IRQ..." << std::endl;

        // 使用 std::atomic_ref (C++20) 或強制轉型處理無鎖變數
        std::atomic<uint32_t>* head = reinterpret_cast<std::atomic<uint32_t>*>(&gpu->head);
        std::atomic<uint32_t>* tail = reinterpret_cast<std::atomic<uint32_t>*>(&gpu->tail);

        uint64_t irq_count;
        while (gpu->running) {
            // 阻塞等待核心的 eventfd 信號 (模擬中斷觸發)
            if (read(irq_fd, &irq_count, sizeof(irq_count)) > 0) {
                
                // 消費者端 Memory Barrier：確保讀取 head/tail 不會被指令重排
                uint32_t current_tail = tail->load(std::memory_order_acquire);
                uint32_t current_head = head->load(std::memory_order_relaxed);

                while (current_head != current_tail) {
                    vgpu_command cmd = gpu->ring[current_head];
                    std::cout << "[Firmware] Executing Command Type: " << cmd.type << std::endl;
                    
                    current_head = (current_head + 1) % RING_BUFFER_SIZE;
                }
                
                // 更新 head 讓 Kernel 知道空間已釋放
                head->store(current_head, std::memory_order_release);
            }
        }
        close(irq_fd);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}