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
    utils/kafka_producer.cpp
    db/database_manager.cpp
    reactor/event_loop.cpp
    reactor/channel.cpp
    reactor/epoller.cpp
)

add_executable(chat_server ${SOURCES})

if(WIN32)
  target_link_libraries(chat_server PRIVATE ws2_32)
endif()

target_include_directories(chat_server
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/third_party/librdkafka/src
)

target_link_libraries(chat_server
    PRIVATE
    Threads::Threads
    sqlite3
    rdkafka
)

install(TARGETS chat_server
    RUNTIME DESTINATION bin
)
