cmake_minimum_required(VERSION 3.10)
project(ChatroomServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -g")

find_package(Threads REQUIRED)
find_package(GTest REQUIRED)

add_library(sqlite3 ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite)

enable_testing()
add_subdirectory(tests)

# Disable librdkafka tests and examples
set(RDKAFKA_BUILD_EXAMPLES OFF CACHE BOOL "Don't build librdkafka examples" FORCE)
set(RDKAFKA_BUILD_TESTS OFF CACHE BOOL "Don't build librdkafka tests" FORCE)


add_subdirectory(src)
add_subdirectory(third_party/librdkafka)

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}") # 构建目录 build 作为安装目录

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/static/
    DESTINATION bin/static)

install(FILES
    ${CMAKE_BINARY_DIR}/third_party/librdkafka/src/librdkafka.so.1
    ${CMAKE_BINARY_DIR}/third_party/librdkafka/src/librdkafka.so
    DESTINATION bin)
