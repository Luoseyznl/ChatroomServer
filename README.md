## main 主循环
- 初始化日志、服务器，注册终止信号处理函数（通过全局指针访问ChatroomServer）后进入空闲等待

## ChatroomServer 聊天室服务器
- 负责管理 HttpServer服务器、dbManager管理器、静态资源static_dir_path目录
- 注册 http 业务路由（httpServer_->addHandler）：
  - `GET` `/`：返回静态资源目录下的 login.html
  - `GET` `/*`：返回静态资源目录下的文件 chat.html register.html
  - `POST` `/register`：处理注册请求，返回注册结果
  - `POST` `/login`：处理登录请求，返回登录结果
  - `POST` `/create_room`：处理创建聊天室请求，返回创建结果
  - `POST` `/join_room`：处理加入聊天室请求，返回加入结果
  - `POST` `/send_message`：处理发送消息请求，返回发送结果
  - `POST` `/messages`：处理获取消息请求，返回消息列表
  - `GET` `/users`：返回在线用户列表
  - `POST` `/logout`：处理登出请求，返回登出结果

## HttpServer 服务器主控类
- 负责监听端口，通过线程池并发处理 http 连接
- 通过哈希表 **handlers_** 存储 method -> path -> RequestHandler
- 提供 handleClient 处理业务
  1. 读取 client 发送的 request 报文
  2. 通过 findHandler 查找对应的业务逻辑
  3. 写回 response 报文

## HttpRequest HTTP请求报文
 - `HttpRequest request = HttpRequest::parse(buffer);`
   - request.method = GET;      - request.method = POST;
   - request.path = /users;     - request.path = /login;
   - request.body = "";         - request.body = "username=alice&password=123456";

## HttpResponse HTTP响应报文
- `HttpResponse response = HttpResponse::createResponse(request);`
  - response.status = 200;     - response.status = 404;
  - response.body = "OK";      - response.body = "Not Found";
  - response.headers = { "Content-Type": "text/plain" };

## DbManager 数据库管理器
- 负责与 SQLite 数据库交互，提供用户、聊天室、消息等数据的增删改查接口

## ThreadPool 线程池
- 预先创建若干工作线程，循环等待各自任务队列里的任务，任务入队后通过条件变量唤醒线程

---

## Epoller 事件多路复用器
- 基于 epoll 高效监听大量 fd，关注 fd 的可读、可写、异常等“感兴趣”事件
  1. 事件注册、修改、删除：将 fd 和感兴趣的事件从 epollFd 和 fdEventsMap_ 中添加、修改、删除
  2. 等待和返回事件：调用 epoll_wait 阻塞等待并存储事件

## Channel 通道
- 事件分发和回调管理的核心组件，channel 与 fd、事件、回调函数绑定，根据活跃事件调用对应的回调函数
  1. 注册事件：通过 setEvents 设置关心的事件，通过 setReadCallback 等设置回调
  2. 事件监测：epoll 检测到 fd 有事件发生，EventLoop 设置 revents_ 并调用 handleEvent()
  3. 事件分发：handleEvent() 根据 revents_ 的类型调用对应的回调函数

## EventLoop 事件循环
- 事件驱动网络编程的核心组件
  1. 事件循环：调用 epoller_.wait（通过 Epoller 类实现）所注册的 fd 上的事件
  2. 事件分发：遍历活跃事件，根据事件的 fd 找到对应的 channel 调用对应的回调函数
  3. 管理 Channel：注册、注销、更新 Channel 的事件监听

## ChatroomServerEpoll 聊天室服务器基于 Epoller 的实现
- 不再使用 HttpServer，基于 epoll/reactor 模型实现的聊天室服务器主类
- 创建监听 socket，绑定端口，并使用 channel 绑定可读事件以及回调（处理客户端连接）
- 将监听 socket 的 channel 注册到 epoller 中，开始事件循环
- 处理客户端连接：接受客户端连接，创建 clientChannel 设置可读事件和回调并注册到 epoller 中
- 处理客户端事件：读取客户端请求，解析请求报文，通过路由 route_ 进行分发处理
- 短链接：处理完请求后关闭连接，避免长时间占用资源

---

## wrk 性能测试
- `wrk` 是一个现代 HTTP 压力测试工具，支持多线程和 Lua 脚本
- 使用 Lua 脚本定义请求参数和处理逻辑
    ```lua
    wrk.method = "POST"
    wrk.headers["Content-Type"] = "application/json"
    wrk.body = '{"username":"testuser","password":"testpass"}'
    ```
    ```sh
    wrk -t4 -c100 -d10s -s post_login.lua http://localhost:8080/login
    ```
    ```
    Running 10s test @ http://localhost:8080/login
        4 threads and 100 connections
        Thread Stats   Avg      Stdev     Max   +/- Stdev
        Latency    10.34ms   10.43ms 131.77ms   94.73%
        Req/Sec     2.76k   778.15     4.03k    67.75%
        110015 requests in 10.01s, 13.64MB read
    Requests/sec:  10993.69
    Transfer/sec:      1.36MB
    ```

## AddressSanitizer (ASan) 检测
- CMakeLists.txt 中启用 AddressSanitizer
    ```cmake
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g")
    ```
- 编译后运行时，ASan 会自动检测内存错误并报告
    ```sh
    ==2493372==ERROR: AddressSanitizer: heap-use-after-free on address 0x50e0000003d8
        #0  reactor::Channel::handleEvent() .../channel.cpp:25
        #1  reactor::EventLoop::loop() .../event_loop.cpp:24
        ...
        freed by thread T0 here:
        #0  operator delete(void*, unsigned long) ...
        #1  ... ChatroomServerEpoll::handleClientEvent(int) ...

    ```
    - EventLoop 访问已释放的 Channel 对象，导致 use-after-free。
      - 原代码释放 channel： chatroom_server_epoll.cpp
        ```cpp
        ...(channel 新客户端连接处理回调函数)...
        eventLoop_->removeChannel(clientFd);
        clientChannels_.erase(fd);
        close(fd);
        ```
    - 解决方法：
      1. 在处理回调函数先暂存待删除的客户端链接 pendingDeleteFds
        ```cpp
        void ChatroomServerEpoll::cleanupPendingChannels() {
        for (int fd : pendingDeleteFds_) {
            clientChannels_.erase(fd);
            close(fd);
        }
        pendingDeleteFds_.clear();
        }
        ```
      2. 在 EventLoop 的 loop() 方法中统一清理已废弃的 Channel
        ```cpp
        Event::loop() {
            ...

            if (chatroomServerEpoll_) {
            chatroomServerEpoll_->cleanupPendingChannels();
            }
        }
        ```

## 总结
- 服务器每秒可处理约 1.1 万个请求，平均响应时间 10.34ms，每秒传输 1.36MB 数据
