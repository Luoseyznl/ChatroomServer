set(SOURCES
    main.cpp
    chatroom_server.cpp
    chatroom_server_epoll.cpp
    http/http_server.cpp
    http/http_request.cpp
    http/http_response.cpp
    chat/user.cpp
    utils/thread_pool.cpp
    utils/logger.cpp
    utils/timer.cpp
    db/database_manager.cpp
    reactor/event_loop.cpp
    reactor/channel.cpp
    reactor/epoller.cpp
)

add_executable(chat_server ${SOURCES})

target_include_directories(chat_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party
)

target_link_libraries(chat_server PRIVATE
    Threads::Threads
    SQLite::SQLite3
)

install(TARGETS chat_server
    RUNTIME DESTINATION bin
)
