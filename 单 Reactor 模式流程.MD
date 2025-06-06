# 1. 服务器启动 (ReactorServer -> EventLoop)

```
ReactorServer::run()
    |
    |-- event_loop_->start()
    |           |
    |           +-- thread_ = std::thread(&EventLoop::loop, this)
    |                    |
    |                    +-- EventLoop::loop()
    |
    +-- accept_thread_ = std::thread(&ReactorServer::acceptLoop, this)
         |
         +-- ReactorServer::acceptLoop()
```

# 2. 新连接处理 (ReactorServer -> EventLoop -> Connection)

```
acceptLoop()
    |
    |-- accept()
    |
    |-- std::make_shared<Connection>()
    |           |
    |           +-- Connection(fd, requestHandler)
    |
    +-- event_loop_->addConnection()
                |
                |-- epoll_ctl(EPOLL_CTL_ADD)
                |
                +-- connections_[fd] = conn
```

# 3. 客户端请求处理 (EventLoop -> Connection <- HttpRequest)

```
EventLoop::loop()
    |
    |-- epoll_wait()
    |
    |-- 检测到读事件
    |   |
    |   +-- connections_[fd]->handleRead()
    |           |
    |           |-- updateActivityTime()
    |           |
    |           |-- readData()
    |           |   |
    |           |   +-- read(sockfd_, buffer, size)
    |           |
    |           +-- handleRequest()
    |               |
    |               |-- HttpRequest::parse()
    |               |
    |               |-- request_handler_() <-- ReactorServer::handleRequest()
    |               |
    |               +-- handleResponse()
    |                   |
    |                   +-- response_buffer_ += response.toString()
    |                   |
    |                   +-- writeData()
    |
    |-- 检测到写事件
    |   |
    |   +-- connections_[fd]->handleWrite()
    |           |
    |           |-- updateActivityTime()
    |           |
    |           +-- writeData()
    |               |
    |               +-- write(sockfd_, response_buffer_.data(), size)
    |
    +-- 修改事件监听
        |
        +-- modifyEvents()
            |
            +-- epoll_ctl(EPOLL_CTL_MOD)
```

# 4. 超时检查流程 (EventLoop -> Connection)

```
EventLoop::loop()
    |
    +-- checkTimeouts()
        |
        +-- for (const auto& pair : connections_)
        |   |
        |   +-- if (pair.second->isTimedOut())
        |       |
        |       +-- removeConnection(pair.first)
        |
        +-- removeConnection()
            |
            |-- epoll_ctl(EPOLL_CTL_DEL)
            |
            |-- conn->handleClose()
            |            |
            |            +-- close(sockfd_)
            |            |
            |            +-- is_closed_ = true
            |
            +-- connections_.erase(fd)
```

# 5. 关闭服务器 (ReactorServer -> EventLoop)

```
ReactorServer::stop()
    |
    |-- running_ = false
    |
    |-- accept_thread_.join()
    |
    +-- event_loop_->stop()
        |
        |-- running_ = false
        |
        |-- thread_.join()
        |
        +-- for (const auto& pair : connections_)
            |
            +-- pair.second->handleClose()
                |
                +-- close(sockfd_)
                |
                +-- is_closed_ = true
```