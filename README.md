# C++ Web Chat Server

A real-time chat system implemented with C++ backend and web frontend.

## Features
- User authentication
- Chat room creation and management
- Real-time text messaging
- File transfer support
- HTTP web server functionality
- Chat history storage in JSON format

## Dependencies
- C++17 or higher
- CMake 3.10 or higher
- nlohmann/json (JSON for Modern C++)
- sqlite3

## Project Structure
```
.
├── src/                    # C++ source files
├── include/               # Header files
├── static/               # Static web files (HTML, CSS, JS)
├── build/                # Build directory
└── CMakeLists.txt       # CMake configuration
```

## Build Instructions
1. Create build directory:
```bash
mkdir build && cd build
```

2. Generate build files:
```bash
cmake ..
```

3. Build the project:
```bash
make install
```

4. Run the server:
```bash
cd bin
./chat_server
```

## License
MIT License
