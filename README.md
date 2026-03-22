# vNPU-Sim (Virtual NPU Simulator)

這是一個虛擬神經處理單元 (Virtual NPU) 的軟硬體協同模擬器，包含負責模擬硬體底層的 Linux 核心模組 (Kernel Module)、負責執行硬體邏輯的韌體 (Firmware)，以及用來與硬體溝通的驅動程式端點 (Driver Client) 與 Python 測試腳本。

## 系統需求
- Linux 作業系統 (建議 Ubuntu 20.04/22.04)
- GCC / G++ 編譯器
- CMake (版本 >= 3.14)
- Python 3
- Linux Kernel Headers (用以編譯 Kernel Module)

## 編譯與安裝

### 1. 編譯並載入 Kernel Module
進入 kernel_module 目錄，編譯驅動程式模組並將其載入核心中。
```bash
cd kernel_module
make
sudo insmod vnpu_drv.ko
```
確認裝置節點已經建立：
```bash
ls -l /dev/vnpu0
# 若權限不足，請執行 sudo chmod 666 /dev/vnpu0
```

### 2. 編譯 C++ 執行檔 (Firmware & Driver)
回到專案根目錄，使用 CMake 進行編譯。
```bash
mkdir build
cd build
cmake ..
make
```

## 執行流程

本專案需要同時執行三個不同的組件，建議開啟三個獨立的終端機視窗。

### 步驟一：啟動 Firmware
Firmware 負責模擬 NPU 內部運作，掛載共享記憶體並開啟 TCP 伺服器 (Port 8080) 等待資料。
```bash
cd build
./firmware
```
*(啟動後放著不需關閉，這代表 NPU 處於運作狀態)*

### 步驟二：透過 Python App 寫入權重並等待結果
這支腳本會透過 TCP 連線，將矩陣資料寫入 Firmware 的記憶體中，然後等待計算結束讀取結果。
```bash
cd scripts
python3 app.py
```
*(此時程式會卡在 `Press Enter after firmware completes calculation...` 進行等待)*

### 步驟三：透過 Driver 發送運算指令
啟動 Driver Client，要求 NPU 啟動硬體級的矩陣乘法運算。
```bash
cd build
./driver 0
```
在跳出的選單中輸入 `1`，並按下 Enter 觸發矩陣運算。

### 步驟四：驗證結果
切換回剛剛執行 Python 的終端機視窗，按下 `Enter` 鍵。Python 將自動讀取 NPU 的記憶體並印出計算結果 `Result Matrix C`。若數值與 `Expected` 的結果一致，即代表完整軟硬體互動流程運作成功。

## 測試環境卸載
測試結束後，記得將核心模組卸載：
```bash
sudo rmmod vnpu_drv
```


