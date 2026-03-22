// g++ driver.cpp -o driver -lpthread
// ./driver 0
// ./driver 1
#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <cstring>
#include "vgpu_common.h"

int tenant_id = -1;
int dev_fd = -1;

// 核心改變：不再透過 mmap 修改 Ring Buffer，而是透過 ioctl 系統呼叫交給 Kernel 處理
void send_command(vgpu_command cmd) {
    int ret = ioctl(dev_fd, VGPU_IOCTL_SUBMIT_CMD, &cmd);
    
    if (ret < 0) {
        // Kernel 會處理背壓 (Backpressure)，若 Ring Buffer 滿了會回傳 EBUSY
        if (errno == EBUSY) {
            std::cout << "[Driver Client] Buffer full. Command rejected by Kernel." << std::endl;
        } else {
            perror("[Driver Client] IOCTL failed");
        }
    } else {
        std::cout << "[Driver Client] Success: Command submitted to Kernel" << std::endl;
    }
}

void print_menu() {
    std::cout << "\n===== vGPU Driver Client (Tenant " << tenant_id << ") =====" << std::endl;
    std::cout << "1. Draw Rect (Test Rendering)" << std::endl;
    std::cout << "2. Clear Screen" << std::endl;
    std::cout << "3. Test ARM64 Assembly (Addition)" << std::endl;
    std::cout << "9. SIMULATE HANG (Test Watchdog)" << std::endl;
    std::cout << "0. Exit" << std::endl;
    std::cout << "Select: ";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./driver_client <tenant_id 0-1>" << std::endl;
        return 1;
    }
    tenant_id = atoi(argv[1]);

    // 開啟 Kernel Character Device
    dev_fd = open("/dev/vgpu0", O_RDWR);
    if (dev_fd < 0) { 
        std::cerr << "Error: Cannot open /dev/vgpu0. Is the Kernel Module loaded?" << std::endl; 
        return 1; 
    }
    
    int choice;
    while (true) {
        print_menu();
        if (!(std::cin >> choice)) break;
        if (choice == 0) break;

        vgpu_command cmd;
        memset(&cmd, 0, sizeof(cmd)); // 防呆清空

        switch(choice) {
            case 1:
                cmd.type = 2; // 對應 CMD_DRAW_RECT
                cmd.params[0] = rand() % (640 - 50); 
                cmd.params[1] = rand() % (480 - 50);
                cmd.params[2] = 50; cmd.params[3] = 50;
                cmd.params[4] = 0xFF00FF00;
                send_command(cmd);
                break;
            case 2:
                cmd.type = 1; // 對應 CMD_CLEAR
                cmd.params[0] = 0xFF222222;
                send_command(cmd);
                break;
            case 3:
                cmd.type = 4; // 對應 CMD_CHECKSUM
                cmd.params[0] = 100;
                cmd.params[1] = 250;
                send_command(cmd);
                break;
            case 9:
                cmd.type = 9; // 對應 CMD_HANG
                send_command(cmd);
                break;
        }
    }

    close(dev_fd);
    return 0;
}