cmake_minimum_required(VERSION 3.14)
project(vGPUSimulator VERSION 1.0)

# 強制要求 C++17 標準
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 開啟嚴格的編譯警告
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wpedantic")

# 韌體執行檔
add_executable(firmware firmware.cpp)
target_include_directories(firmware PRIVATE ${CMAKE_SOURCE_DIR})

# 單元測試 (Google Test) 整合配置
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        release-1.12.1
)
FetchContent_MakeAvailable(googletest)

add_executable(vgpu_tests tests/test_ringbuffer.cpp)
target_link_libraries(vgpu_tests gtest_main)