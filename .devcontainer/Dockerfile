FROM ubuntu:22.04

# 避免交互
ENV DEBIAN_FRONTEND=noninteractive

# 定义可选代理参数
ARG http_proxy
ARG https_proxy

# 如果构建时传入了代理，ENV 会生效
ENV http_proxy=${http_proxy}
ENV https_proxy=${https_proxy}

# 软件安装
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y \
    build-essential \
    cmake \
    gdb \
    git \
    vim \
    zsh \
    wget \
    curl \
    sudo \
    pkg-config \
    clang \
    llvm \
    valgrind \
    tcpdump \
    sqlite3 \
    libsqlite3-dev \
    libgoogle-glog-dev \
    libprotobuf-dev \
    protobuf-compiler \
    libgtest-dev \
    libbenchmark-dev \
    nginx && \
    # 清理缓存避免镜像过大
    rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /projects/ChatroomServer

# 默认执行 bash
CMD ["bash"]

