import socket
import struct
import numpy as np
import streamlit as st
import subprocess
import time

SERVER_IP = '127.0.0.1'
SERVER_PORT = 8080
HEADER_SIZE = 32
NPU_MEM_BYTES = 640 * 480 * 4

def send_weights(offset, matrix):
    data_bytes = matrix.astype(np.float32).tobytes()
    size_in_bytes = len(data_bytes)
    
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((SERVER_IP, SERVER_PORT))
        s.sendall(struct.pack('<B I I', 1, offset, size_in_bytes))
        s.sendall(data_bytes)

def read_npu_state():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((SERVER_IP, SERVER_PORT))
        s.sendall(struct.pack('<B', 0))
        
        expected_size = HEADER_SIZE + NPU_MEM_BYTES
        data = bytearray()
        while len(data) < expected_size:
            packet = s.recv(8192)
            if not packet:
                break
            data.extend(packet)
            
        return bytes(data)

def trigger_hardware():
    # Invoke driver_client, input '1' (Matrix Multiply) then '0' (Exit)
    process = subprocess.Popen(
        ['./build/driver', '0'],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    stdout, stderr = process.communicate(input="1\n0\n")
    return stdout

st.title("vNPU-Sim System Monitoring Dashboard")

dim = st.sidebar.number_input("Matrix Dimension", min_value=2, max_value=16, value=4, step=1)

if "mat_A" not in st.session_state:
    st.session_state.mat_A = np.random.rand(dim, dim).astype(np.float32)
    st.session_state.mat_B = np.random.rand(dim, dim).astype(np.float32)

st.subheader("1. Load Weights")
col1, col2 = st.columns(2)
with col1:
    st.write("Matrix A")
    st.dataframe(st.session_state.mat_A)
with col2:
    st.write("Matrix B")
    st.dataframe(st.session_state.mat_B)

if st.button("Send Weights to vNPU Memory"):
    try:
        # Offset for A is 0, offset for B is dim*dim*4 bytes
        send_weights(0, st.session_state.mat_A)
        send_weights(dim * dim * 4, st.session_state.mat_B)
        st.success("Weights successfully written to NPU shared memory.")
    except ConnectionRefusedError:
        st.error("Connection refused. Ensure the Firmware is running.")

st.subheader("2. Trigger Hardware Execution")
if st.button("Submit Command via Driver"):
    output = trigger_hardware()
    st.text_area("Driver Output", output, height=150)
    st.success("Command submitted to Kernel Ring Buffer.")

st.subheader("3. Verify Results")
if st.button("Read Results from Shared Memory"):
    try:
        state_data = read_npu_state()
        npu_mem_bytes = state_data[HEADER_SIZE : HEADER_SIZE + NPU_MEM_BYTES]
        mat_C_raw = np.frombuffer(npu_mem_bytes, dtype=np.float32)
        
        # Offset for C is set after A and B, e.g., dim*dim*8 bytes
        offset_c = dim * dim * 2
        mat_C = mat_C_raw[offset_c : offset_c + (dim * dim)].reshape(dim, dim)
        
        expected_C = np.dot(st.session_state.mat_A, st.session_state.mat_B)
        
        col3, col4 = st.columns(2)
        with col3:
            st.write("vNPU Result (Matrix C)")
            st.dataframe(mat_C)
        with col4:
            st.write("Expected Result (NumPy)")
            st.dataframe(expected_C)
            
        mse = ((mat_C - expected_C)**2).mean()
        st.metric(label="Mean Squared Error (MSE)", value=f"{mse:.6f}")
        
    except ConnectionRefusedError:
        st.error("Connection refused. Cannot read NPU state.")