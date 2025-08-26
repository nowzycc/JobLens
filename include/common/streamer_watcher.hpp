#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LOGGER_TRACE

#include <functional>
#include <memory>
#include <string>

class StreamWatcher {
public:
    // 统一回调签名：buf 为本次可读到的数据，len 为数据长度
    using Callback = std::function<void(const char* buf, std::size_t len)>;

    enum class Type {
        TCP,      // 监听某个 TCP 端口，有连接到达后监视该连接的 socket
        FIFO,     // 监听 Linux 管道（mkfifo）
        FILE      // 监视一个普通文件，检测追加内容（inotify）
    };

    struct Config {
        Type type;
        std::string path;   // 对于 FIFO/FILE 是文件路径；对于 TCP 是 "host:port"
    };

    StreamWatcher(const Config& cfg, Callback cb);
    ~StreamWatcher();

    void start();   // 启动事件循环（内部线程）
    void stop();    // 停止事件循环，可重复 start/stop

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};