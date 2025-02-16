#include "chat_server.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

ThreadPool::ThreadPool(size_t num_threads) : stop(false) {
    for(size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this] {
            while(true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    condition.wait(lock, [this] { 
                        return stop || !tasks.empty(); 
                    });
                    if(stop && tasks.empty()) return;
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(std::thread &worker: workers) {
        worker.join();
    }
}

template<class F>
void ThreadPool::enqueue(F&& f) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        tasks.emplace(std::forward<F>(f));
    }
    condition.notify_one();
}

ChatServer::ChatServer(int port) 
    : port(port), running(false), thread_pool(std::thread::hardware_concurrency()) {
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOG_ERROR << "Failed to create socket";
        throw std::runtime_error("Failed to create socket");
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        LOG_ERROR << "Failed to set socket options";
        throw std::runtime_error("Failed to set socket options");
    }
    
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        LOG_ERROR << "Failed to bind to port";
        throw std::runtime_error("Failed to bind to port");
    }
    
    if (listen(server_fd, 10) < 0) {
        LOG_ERROR << "Failed to listen";
        throw std::runtime_error("Failed to listen");
    }
}

ChatServer::~ChatServer() {
    running = false;
    close(server_fd);
}

void ChatServer::run() {
    running = true;
    LOG_INFO << "Server listening on port " << port;
    
    while (running) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) {
            LOG_ERROR << "Failed to accept connection";
            continue;
        }
        
        thread_pool.enqueue([this, client_fd] {
            handle_client(client_fd);
        });
    }
}

void ChatServer::handle_client(int client_fd) {
    char buffer[4096];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        LOG_DEBUG << "Raw request (" << bytes_read << " bytes):";
        LOG_DEBUG << std::string(80, '-');
        LOG_DEBUG << buffer;
        LOG_DEBUG << std::string(80, '-');
        
        HttpRequest request = parse_request(std::string(buffer));
        LOG_DEBUG << "Parsed request:";
        LOG_DEBUG << "Method: " << request.method;
        LOG_DEBUG << "Path: " << request.path;
        LOG_DEBUG << "Headers:";
        for (const auto& header : request.headers) {
            LOG_DEBUG << "  " << header.first << ": " << header.second;
        }
        LOG_DEBUG << "Body: " << request.body;
        
        HttpResponse response = handle_request(request);
        std::string response_str = response.toString();
        
        LOG_DEBUG << "Sending response:";
        LOG_DEBUG << std::string(80, '-');
        LOG_DEBUG << response_str;
        LOG_DEBUG << std::string(80, '-');
        
        ssize_t bytes_written = write(client_fd, response_str.c_str(), response_str.length());
        LOG_DEBUG << "Wrote " << bytes_written << " bytes to client";
    } else {
        LOG_ERROR << "Error reading from client: " << strerror(errno);
    }
    
    close(client_fd);
}

HttpRequest ChatServer::parse_request(const std::string& request_str) {
    HttpRequest request;
    std::istringstream iss(request_str);
    
    // Parse request line
    std::string line;
    if (!std::getline(iss, line)) {
        LOG_ERROR << "Failed to read request line";
        return request;
    }
    
    // Remove carriage return if present
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    
    std::istringstream request_line(line);
    request_line >> request.method >> request.path;
    LOG_DEBUG << "Parsed request line: " << line;
    
    // Parse headers
    size_t content_length = 0;
    while (std::getline(iss, line)) {
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        // Empty line marks the end of headers
        if (line.empty()) {
            break;
        }
        
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            // Trim leading/trailing whitespace from value
            value.erase(0, value.find_first_not_of(" "));
            value.erase(value.find_last_not_of(" ") + 1);
            request.headers[key] = value;
            
            LOG_DEBUG << "Parsed header: " << key << " = " << value;
            
            // Get Content-Length
            if (key == "Content-Length") {
                try {
                    content_length = std::stoul(value);
                    LOG_DEBUG << "Content-Length: " << content_length;
                } catch (const std::exception& e) {
                    LOG_ERROR << "Invalid Content-Length: " << value;
                }
            }
        }
    }
    
    // Read body if Content-Length is present
    if (content_length > 0) {
        // Get the current position in the stream
        std::streampos body_start = iss.tellg();
        if (body_start != -1) {
            // Calculate remaining bytes in the string
            size_t remaining = request_str.length() - static_cast<size_t>(body_start);
            LOG_DEBUG << "Body start position: " << body_start << ", remaining bytes: " << remaining;
            
            if (remaining >= content_length) {
                request.body = request_str.substr(static_cast<size_t>(body_start), content_length);
                LOG_DEBUG << "Read body of length " << request.body.length();
            } else {
                LOG_ERROR << "Not enough bytes remaining for body. Expected " << content_length 
                          << " but only have " << remaining;
            }
        } else {
            LOG_ERROR << "Failed to get body position in stream";
        }
    }
    
    return request;
}

HttpResponse ChatServer::handle_request(const HttpRequest& request) {
    if (request.path == "/register") {
        return handle_register(request);
    } else if (request.path == "/login") {
        return handle_login(request);
    } else if (request.path == "/create_room") {
        return handle_create_room(request);
    } else if (request.path == "/rooms") {
        return handle_get_rooms(request);
    } else {
        // Serve static files
        HttpResponse response;
        std::string path = request.path == "/" ? "/index.html" : request.path;
        serve_static_file("static" + path, response);
        return response;
    }
}

void ChatServer::serve_static_file(const std::string& path, HttpResponse& response) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        response.status_code = 404;
        response.body = "File not found";
        return;
    }
    
    std::string ext = path.substr(path.find_last_of('.') + 1);
    if (ext == "html") {
        response.headers["Content-Type"] = "text/html";
    } else if (ext == "css") {
        response.headers["Content-Type"] = "text/css";
    } else if (ext == "js") {
        response.headers["Content-Type"] = "application/javascript";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    response.status_code = 200;
    response.body = buffer.str();
}

HttpResponse ChatServer::handle_register(const HttpRequest& request) {
    Json json = Json::parse(request.body);
    std::string username = json.data["username"];
    std::string password = json.data["password"];
    
    std::lock_guard<std::mutex> lock(users_mutex);
    if (users.find(username) != users.end()) {
        return {409, "User already exists"};
    }
    
    users[username] = User{username, password, false};
    return {200, "Registration successful"};
}

HttpResponse ChatServer::handle_login(const HttpRequest& request) {
    Json json = Json::parse(request.body);
    std::string username = json.data["username"];
    std::string password = json.data["password"];
    
    std::lock_guard<std::mutex> lock(users_mutex);
    auto it = users.find(username);
    if (it == users.end() || it->second.password != password) {
        return {401, "Invalid credentials"};
    }
    
    it->second.is_online = true;
    return {200, "Login successful"};
}

HttpResponse ChatServer::handle_create_room(const HttpRequest& request) {
    LOG_INFO << "Handling create room request with body: " << request.body;
    
    try {
        Json json = Json::parse(request.body);
        LOG_DEBUG << "Parsed JSON";
        
        if (!json.data.contains("name") || !json.data.contains("creator")) {
            LOG_ERROR << "Missing required fields";
            return {400, "Missing required fields: name and creator"};
        }
        
        std::string room_name = json.data["name"];
        std::string creator = json.data["creator"];
        
        LOG_INFO << "Creating room: " << room_name << " by " << creator;
        
        std::lock_guard<std::mutex> lock(rooms_mutex);
        if (chat_rooms.find(room_name) != chat_rooms.end()) {
            LOG_WARN << "Room already exists";
            return {409, "Room already exists"};
        }
        
        chat_rooms[room_name] = ChatRoom{room_name, creator, {creator}, {}};
        load_chat_history(room_name);
        LOG_INFO << "Room created successfully";
        return {200, "Room created successfully"};
    } catch (const std::exception& e) {
        LOG_ERROR << "Error creating room: " << e.what();
        return {400, std::string("Error creating room: ") + e.what()};
    }
}

HttpResponse ChatServer::handle_get_rooms(const HttpRequest& request) {
    std::lock_guard<std::mutex> lock(rooms_mutex);
    std::vector<Json> room_list;
    
    for (const auto& room : chat_rooms) {
        Json room_json;
        room_json.data["name"] = room.first;
        room_json.data["creator"] = room.second.creator;
        room_list.push_back(room_json);
    }
    
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < room_list.size(); ++i) {
        if (i > 0) ss << ",";
        ss << room_list[i].dump();
    }
    ss << "]";
    
    return {200, ss.str()};
}

void ChatServer::save_chat_history(const std::string& room) {
    std::string filename = "data/" + room + "_history.json";
    std::ofstream file(filename);
    if (file.is_open()) {
        auto it = chat_rooms.find(room);
        if (it != chat_rooms.end()) {
            std::stringstream ss;
            ss << "[";
            for (size_t i = 0; i < it->second.messages.size(); ++i) {
                if (i > 0) ss << ",";
                ss << it->second.messages[i].dump();
            }
            ss << "]";
            file << ss.str();
        }
    }
}

void ChatServer::load_chat_history(const std::string& room) {
    std::string filename = "data/" + room + "_history.json";
    std::ifstream file(filename);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        // Simple array parsing
        size_t pos = 1;  // Skip first '['
        while (pos < content.length() && content[pos] != ']') {
            size_t end = content.find("}", pos);
            if (end == std::string::npos) break;
            
            std::string message_str = content.substr(pos, end - pos + 1);
            chat_rooms[room].messages.push_back(Json::parse(message_str));
            
            pos = content.find("{", end);
            if (pos == std::string::npos) break;
        }
    }
}

void ChatServer::broadcast_to_room(const std::string& room, const Json& message) {
    std::lock_guard<std::mutex> lock(rooms_mutex);
    auto it = chat_rooms.find(room);
    if (it != chat_rooms.end()) {
        it->second.messages.push_back(message);
        save_chat_history(room);
    }
}
