#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LOGGER_TRACE

#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <optional>

#include "common/config.hpp"

class JobStarter {
public:
    // 回调类型：pid + exit_code
    using OnExit = std::function<void(int pid, int exit_code)>;

    // 配置结构体，便于将来扩展
    struct Options {
        std::string            exe;          // 可执行文件路径
        std::vector<std::string> args;       // 传给子进程的命令行参数
        std::optional<std::chrono::milliseconds> timeout; // 可选超时
    };

    JobStarter(){
        // 初始化工作线程池
        auto global_config = Config::instance();
    };
    // ~JobStarter() { shutdown(); }

    // // 非拷贝、可移动
    // JobStarter(const JobStarter&)            = delete;
    // JobStarter& operator=(const JobStarter&) = delete;
    // JobStarter(JobStarter&&)                 = default;
    // JobStarter& operator=(JobStarter&&)      = default;

    // 注册回调（线程安全）
    void setCallback(OnExit cb);

    // 启动一次作业，返回是否成功
    [[nodiscard]] bool launch(const Options& opt);

    // 强制结束所有正在运行的作业并等待线程退出
    void shutdown();
    // 获取单例实例
    static JobStarter& instance() {
        static auto instance = JobStarter();
        return instance;
    }
    pid_t getChildPID() const {
        return childPID;
    }
private:
    // 真正的工作线程函数
    void worker(int pid,
                OnExit cb);
    pid_t childPID;
    std::mutex              mtx_;
    OnExit                  callback_;          // 外部回调
    std::thread worker_;          // 工作线程
    std::atomic<bool>       shutdown_{false};   // 全局关闭标志
};