cmake_minimum_required(VERSION 3.10)
project(chat_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加调试选项
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")

# Find required packages

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
    ${SQLite3_INCLUDE_DIRS}
)

# 设置安装目录为 build 目录
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}")

# Add subdirectories
add_subdirectory(src)

# Copy static files to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/static/ DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
