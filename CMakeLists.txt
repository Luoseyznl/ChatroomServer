cmake_minimum_required(VERSION 3.10)
project(ChatroomServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")

find_package(Threads REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(GTest REQUIRED)

add_subdirectory(src)

enable_testing()

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}") # 构建目录
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/static/ DESTINATION ${CMAKE_BINARY_DIR}/src/static)

# 添加测试目录
# add_subdirectory(tests)

# 添加测试可执行文件
# add_executable(test_logger tests/test_logger.cpp)
# target_link_libraries(test_logger PRIVATE 
#     GTest::gtest 
#     GTest::gtest_main 
#     Threads::Threads 
#     SQLite::SQLite3
# )