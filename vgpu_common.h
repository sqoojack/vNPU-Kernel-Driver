#ifndef VGPU_COMMON_H
#define VGPU_COMMON_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define VGPU_MAGIC 'V'
#define VGPU_IOCTL_SUBMIT_CMD _IOW(VGPU_MAGIC, 1, struct vgpu_command)
#define VGPU_IOCTL_SET_EVENTFD _IOW(VGPU_MAGIC, 2, int)

#define RING_BUFFER_SIZE 256
#define DMA_PAGE_SIZE 4096 // 模擬 Page Alignment

// 指令結構
struct vgpu_command {
    __u32 type;
    __u32 params[5];
};

// 狀態與 Ring Buffer，強制 4K 頁面對齊以符合 mmap 需求
struct vgpu_shared_state {
    __u32 magic;
    __u32 running;
    
    // Ring Buffer 必須使用記憶體屏障保護
    __u32 head; 
    __u32 tail; 
    struct vgpu_command ring[RING_BUFFER_SIZE];

    // 模擬 4K 對齊的 DMA 區域
    __u8 dma_buffer[DMA_PAGE_SIZE] __attribute__((aligned(DMA_PAGE_SIZE)));
};

#endif