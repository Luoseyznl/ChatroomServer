cmake_minimum_required(VERSION 3.10)
project(chat_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON) # 强制要求使用指定的 C++ 标准，不允许降级

# 设置构建类型为 Debug 模式
set(CMAKE_BUILD_TYPE Debug)
# 添加调试选项：-g 生成调试信息，-O0 禁用优化
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

# 启用地址sanitizer检测内存问题
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")

# 查找并链接线程库（pthread）
find_package(Threads REQUIRED)
# 查找并链接 SQLite3 库
find_package(SQLite3 REQUIRED)

# 设置包含目录（可以被直接包含）
# CMAKE_SOURCE_DIR 表示顶级 CMakeLists.txt 文件所在的目录
# include_directories(
#     ${CMAKE_SOURCE_DIR}/src            # 源代码目录
#     ${CMAKE_SOURCE_DIR}/third_party    # 第三方库目录
#     ${SQLite3_INCLUDE_DIRS}            # SQLite3 头文件目录
# )

# 设置安装目录为构建目录（build）
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}")

# 添加 src 子目录（查找并处理 CMakeLists.txt 文件）
add_subdirectory(src)

# 将静态文件从源目录复制到构建目录
# CMAKE_CURRENT_SOURCE_DIR 表示当前 CMakeLists.txt 文件所在的目录
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/static/ DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/static)