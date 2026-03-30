#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

echo "=== 1. Compile and Load Kernel Module ==="
cd kernel_module
make
# Remove old module if it exists
sudo rmmod vnpu_drv 2>/dev/null || true
sudo insmod vnpu_drv.ko
# Ensure device node permissions
sudo chmod 666 /dev/vnpu0
cd ..

echo "=== 2. Build C++ Firmware and Driver ==="
mkdir -p build
cd build
cmake ..
make
cd ..

echo "=== 3. Launch NPU Firmware ==="
# Run firmware in the background
./build/firmware &
FW_PID=$!

# Wait for TCP Server initialization
sleep 2

echo "=== 4. Launch Streamlit App ==="
# Run the Streamlit application
streamlit run scripts/app.py

# Cleanup background processes and kernel module on exit
kill $FW_PID
sudo rmmod vnpu_drv
echo "System shutdown complete and resources cleaned up."