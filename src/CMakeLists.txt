# Add all source files
set(SOURCES
    main.cpp
    chat_application.cpp
    http/http_server.cpp
    http/http_request.cpp
    http/http_response.cpp
    http/reactor_server.cpp
    http/connection.cpp
    http/event_loop.cpp
    chat/user.cpp
    utils/thread_pool.cpp
    utils/logger.cpp
    utils/work_stealing_thread_pool.cpp
    utils/timer.cpp
    db/database_manager.cpp
)

# 添加可执行文件目标
add_executable(chat_server ${SOURCES})

# 为目标设置私有包含目录
# PRIVATE 表示这些目录只用于编译 chat_server，不会传递给依赖于 chat_server 的其他目标
target_include_directories(chat_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party
)

# 为目标私有链接到线程库和 SQLite3 库
# PRIVATE 表示这些库只用于 chat_server，不会传递给依赖于 chat_server 的其他目标
target_link_libraries(chat_server PRIVATE
    Threads::Threads
    SQLite::SQLite3
)

# 安装目标
# 将编译好的 chat_server 可执行文件安装到 bin 目录
install(TARGETS chat_server
        RUNTIME DESTINATION bin)
