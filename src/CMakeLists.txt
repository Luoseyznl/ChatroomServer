# Add all source files
set(SOURCES
    main.cpp
    chat_application.cpp
    http/http_server.cpp
    http/http_request.cpp
    http/http_response.cpp
    chat/user.cpp
    utils/thread_pool.cpp
    utils/thread_pool.cpp
    utils/work_stealing_thread_pool.cpp
    utils/timer.cpp
    db/database_manager.cpp
)

add_executable(chat_server ${SOURCES})

# Include directories
target_include_directories(chat_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party
)

# Link libraries
target_link_libraries(chat_server PRIVATE
    Threads::Threads
    SQLite::SQLite3
)

# 安装目标
install(TARGETS chat_server
        RUNTIME DESTINATION bin)
