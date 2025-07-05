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

Nginx 反向代理

理解生产环境部署、负载均衡、静态资源分发、HTTPS 等基础。
先学这个有助于你本地和线上都能方便地调试和访问你的服务。
Postman 测试方法

学会用 Postman 测试你的 HTTP API，能快速验证接口正确性，提升开发效率。
这一步和 Nginx 配合，可以模拟真实生产环境的请求。
Kafka 中间件

学习消息队列的基本原理和用法，理解异步解耦、消息持久化、分布式等概念。
可以先用官方客户端，后续再考虑源码或高级用法。
SQLite 源码集成

理解如何直接集成 SQLite 源码，便于移植、定制和优化。
这一步可以让你对数据库的底层实现有更深入的理解。
TSan/ASan 插装分析

学习用 ThreadSanitizer（TSan）和 AddressSanitizer（ASan）检测并发和内存问题。
这一步适合在你的服务基本跑通后，排查潜在 bug。
perf 性能检测

学习用 perf 工具分析程序的 CPU、内存等性能瓶颈。
适合在服务稳定后，做性能优化。
benchmark 插桩分析

学习用 Google Benchmark 或自定义 benchmark 对关键代码做微基准测试。
这一步可以帮助你量化优化效果。
