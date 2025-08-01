#pragma once

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#define SOCKET_CLOSE(fd) closesocket(fd)
#define SOCKET_READ(fd, buf, len) recv(fd, buf, len, 0)
#define SOCKET_WRITE(fd, buf, len) send(fd, buf, len, 0)
#define SOCKET_ERROR_MSG() WSAGetLastError()
#define EINTR WSAEINTR

// Windows 文件操作
#include <sys/stat.h>
#include <sys/types.h>
#define stat _stat

// Windows 错误处理
inline const char* socket_strerror(int errnum) {
  static char buffer[256];
  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errnum, 0, buffer,
                 sizeof(buffer), NULL);
  return buffer;
}

inline bool initSockets() {
  WSADATA wsaData;
  return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}

inline void cleanupSockets() { WSACleanup(); }

#else
#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define SOCKET_CLOSE(fd) close(fd)
#define SOCKET_READ(fd, buf, len) read(fd, buf, len)
#define SOCKET_WRITE(fd, buf, len) write(fd, buf, len)
#define SOCKET_ERROR_MSG() errno

inline const char* socket_strerror(int errnum) { return strerror(errnum); }

inline bool initSockets() { return true; }
inline void cleanupSockets() {}
#endif