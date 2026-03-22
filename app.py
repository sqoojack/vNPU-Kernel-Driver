# python -m streamlit run app.py
import streamlit as st
import mmap
import struct
import numpy as np
import time
import os

# 設定頁面
st.set_page_config(page_title="vGPU Sim", layout="wide")
st.title("🖥️ vGPU Architecture Simulator (macOS/Linux)")

# --- 設定區 ---
SHM_FILENAME = "/dev/vgpu0"
WIDTH = 640
HEIGHT = 480
VRAM_SIZE = WIDTH * HEIGHT * 4

# 格式字串: I (int), f (float), Q (unsigned long long)
HEADER_FMT = "IIIfQI" 
HEADER_SIZE = struct.calcsize(HEADER_FMT)

def get_data():
    if not os.path.exists(SHM_FILENAME):
        return None, None
    
    try:
        with open(SHM_FILENAME, "r+b") as f:
            mm = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
            
            # 1. 讀取標頭
            header_bytes = mm[:HEADER_SIZE]
            header_data = struct.unpack(HEADER_FMT, header_bytes)
            
            # 2. 讀取 VRAM
            vram_offset = HEADER_SIZE
            vram_bytes = mm[vram_offset : vram_offset + VRAM_SIZE]
            
            mm.close()
            return header_data, vram_bytes
    except Exception as e:
        st.error(f"讀取錯誤: {e}")
        return None, None

header, vram_bytes = get_data()

if header:
    # 解包順序要對應 HEADER_FMT
    magic, running, frame, temp, heartbeat, wd_count = header
    
    if magic != 0x56475055:
        st.error(f"記憶體標頭錯誤 (Magic: {hex(magic)}) - 請重新編譯 C++ 並重啟 firmware")
    else:
        col1, col2 = st.columns([3, 1])
        
        with col1:
            st.subheader("VRAM Visualization")
            if vram_bytes:
                # 轉換 BGR 格式
                raw_img = np.frombuffer(vram_bytes, dtype=np.uint8).reshape((HEIGHT, WIDTH, 4))
                bgr_img = raw_img[:, :, :3]
                st.image(bgr_img, channels="BGR", use_container_width=True)
        
        with col2:
            st.subheader("System Telemetry")
            
            # 狀態指示燈
            status_color = "normal"
            if not running: status_color = "off"
            elif temp > 80: status_color = "inverse" # 過熱警告
            
            st.metric("System Status", "Running" if running else "Stopped", 
                     delta="Online" if running else "Offline")
            
            st.metric("Frame Counter", frame)
            
            # 溫度顯示
            st.metric("GPU Temperature", f"{temp:.1f} °C", 
                     delta=f"{temp - 40.0:.1f} °C" if temp > 0 else "0.0", 
                     delta_color="inverse" if temp > 80 else "normal")
            
            # 新增：Watchdog 監控數據
            st.markdown("---")
            st.markdown("### 🛡️ Watchdog Status")
            st.metric("Last Heartbeat", f"{heartbeat % 10000} ts")
            st.metric("Watchdog Resets", f"{wd_count}", 
                     delta_color="inverse" if wd_count > 0 else "off")

        time.sleep(0.1)
        st.rerun()

else:
    st.warning(f"找不到記憶體檔案: {SHM_FILENAME}")
    st.info("請先執行: ./firmware")
    if st.button("重新連線"):
        st.rerun()