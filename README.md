# vNPU-Kernel-Driver: AI-Accelerator-System-Simulator

vNPU-Sim is a hardware-software co-design project that simulates a Neural Processing Unit (NPU) environment. It features a custom Linux Kernel Module for low-latency Inter-Process Communication (IPC), a C++ firmware for hardware logic emulation, and a Python-based application layer for deep learning workload orchestration.

## System Architecture

The project is architected into three distinct layers to mimic real-world AI acceleration stacks:

1.  **Kernel Space (LKM)**: A Linux Kernel Module (`vnpu_drv.ko`) that manages shared memory regions and provides a command submission interface via `ioctl` and `eventfd`. It implements a high-performance, asynchronous communication path between the driver client and the hardware simulator.
2.  **Hardware Emulation (Firmware)**: A C++ process that simulates NPU hardware behavior, including GEMM (General Matrix Multiply) operations, temperature tracking, and a hardware watchdog. It utilizes **Zero-copy mmap** to access the shared memory provided by the kernel.
3.  **User Space (Driver & App)**:
    * **Driver Client**: A C++ implementation that submits hardware commands to the kernel-side ring buffer.
    * **Python App**: Handles high-level data management, such as loading weights/tensors into the NPU's memory space via a custom TCP/IP stack.

## Key Features for AI Systems

* **Zero-Copy Memory Management**: Leverages `remap_vmalloc_range` in the kernel and `mmap` in user space to eliminate data copying overhead between the driver and firmware.
* **Asynchronous Command Submission**: Uses a lock-protected ring buffer and `eventfd` for non-blocking, interrupt-driven command processing.
* **Hardware Reliability Simulation**: Features a SIGSEGV interceptor that generates automated binary crash dumps and reports for post-mortem failure analysis.
* **Multi-Tenant Design**: Supports tenant identification to simulate resource isolation in cloud AI environments.

## Prerequisites

* **OS**: Linux (Tested on Ubuntu 20.04/22.04)
* **Compiler**: GCC/G++ (C++17 support)
* **Build System**: CMake (>= 3.14)
* **Kernel**: Linux Kernel Headers (for LKM compilation)
* **Python**: Python 3.x with NumPy

## Build & Installation

### 1. Compile and Load the Kernel Module
Navigate to the `kernel_module` directory to build the driver:
```bash
cd kernel_module
make
sudo insmod vnpu_drv.ko
# Ensure the device node exists
ls -l /dev/vnpu0
# Grant permissions if necessary: sudo chmod 666 /dev/vnpu0
```


### 2. Build C++ Components (Firmware & Driver)
Use CMake to build the simulator components:
```bash
mkdir build && cd build
cmake ..
make
```


## Execution Workflow

To simulate a complete AI inference cycle, follow these steps in separate terminals:

### Step 1: Launch NPU Firmware
The firmware initializes the shared state, mounts the memory-mapped region, and starts the TCP server for weight loading.
```bash
./build/firmware
```

### Step 2: Load Weights via Python App
The app pushes matrix data (weights) to the NPU memory and waits for the computation trigger.
```bash
python3 scripts/app.py
```

### Step 3: Trigger Hardware Execution
The Driver Client sends an `ioctl` to the kernel, which signals the firmware to begin the GEMM operation.
```bash
./build/driver 0
# Select option '1' to trigger Matrix Multiply (4x4)
```

### Step 4: Verify Results
Return to the Python terminal and press `Enter`. The app will read the resulting matrix from the shared memory and compare it against a local NumPy calculation to verify precision.

## Testing & Stability
* **Stress Testing**: Use `scripts/stress_test.py` to verify ring buffer backpressure and kernel-to-firmware synchronization under high load.
* **Cleanup**: To unload the driver, run `sudo rmmod vnpu_drv`.