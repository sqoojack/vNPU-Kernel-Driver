#ifndef VGPU_COMMON_H
#define VGPU_COMMON_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define VGPU_MAGIC 'V'
#define VGPU_IOCTL_SUBMIT_CMD _IOW(VGPU_MAGIC, 1, struct vgpu_command)
#define VGPU_IOCTL_SET_EVENTFD _IOW(VGPU_MAGIC, 2, int)

#define WIDTH 640
#define HEIGHT 480
#define RING_BUFFER_SIZE 256
#define DMA_PAGE_SIZE 4096

// 指令結構
struct vgpu_command {
    __u32 type;
    __u32 params[5];
};

// 狀態與 Ring Buffer，對應 app.py 的結構解析 (IIIfQI + VRAM)
struct vgpu_shared_state {
    // 遙測數據 (Telemetry)
    __u32 magic;                 // 0x56475055
    __u32 running;
    __u32 frame_counter;
    float temperature;
    __u64 last_heartbeat;
    __u32 watchdog_reset_count;

    // VRAM 影像資料
    __u32 vram[WIDTH * HEIGHT];

    // Ring Buffer 必須使用記憶體屏障保護
    __u32 head; 
    __u32 tail; 
    struct vgpu_command ring[RING_BUFFER_SIZE];

    // 模擬 4K 對齊的 DMA 區域
    __u8 dma_buffer[DMA_PAGE_SIZE] __attribute__((aligned(DMA_PAGE_SIZE)));
};

#endif