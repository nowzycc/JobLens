#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LOGGER_TRACE

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <thread>

// 第三方
#include "utils/nlohmann/json.hpp"
#include <spdlog/spdlog.h>


#include "common/config.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

/*==========================================================
 * 一个非常轻量的业务状态管理器示例
 *=========================================================*/
class StateManager {
public:
    static StateManager& instance();
    void on_promote();
    void on_demote();
    nlohmann::json snapshot();
    void load_snapshot(const nlohmann::json& j);
    void inc();               // 供测试用

private:
    StateManager() = default;
    std::mutex mu_;
    std::string role_{"slave"};
    int value_ = 0;
};

/*==========================================================
 * 分布式节点实现
 *=========================================================*/
class DistributedNode {
public:
    /*---------------- 常数 ----------------*/
    static constexpr int   LEASE_SEC            = 1;
    static constexpr int   PRE_PROMOTE_RATIO    = 30;   // 30 %
    static constexpr auto  HEARTBEAT_INTERVAL   = std::chrono::milliseconds{250};
    static constexpr auto  SLAVE_CHECK_INTERVAL = std::chrono::milliseconds{100};

    /*---------------- 构造/析构 ----------------*/
    DistributedNode();
    ~DistributedNode();

    /* 单例 */
    static DistributedNode& instance();

    /*---------------- 生命周期 ----------------*/
    void start();
    void stop();

    void set_become_master_callback(std::function<void(void)> cb);
    void set_become_slave_callback(std::function<void(void)> cb);

private:
    /*------------------------------------------------------
     * 数据结构
     *----------------------------------------------------*/
    struct Lease {
        uint64_t epoch = 0;
        uint64_t updated_at = 0;
        uint64_t expire_at  = 0;
        nlohmann::json snapshot;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Lease, epoch, updated_at, expire_at, snapshot)
    };

    Lease current_;

    /*------------------------------------------------------
     * 工具函数
     *----------------------------------------------------*/
    static uint64_t epoch_ms();

    /*------------------------------------------------------
     * 锁/租约操作
     *----------------------------------------------------*/
    bool try_acquire_lock();
    Lease read_lease();
    void write_lease(const Lease& l);

    /*------------------------------------------------------
     * 主/从 角色切换
     *----------------------------------------------------*/
    void become_master();
    void become_slave();

    /*------------------------------------------------------
     * PID 文件管理 & 信号通知
     *----------------------------------------------------*/
    void update_slave_pids();
    void notify_slaves(int sig);

    /*------------------------------------------------------
     * 信号处理
     *----------------------------------------------------*/
    void setup_signal_handlers();
    void try_promote();

    /*------------------------------------------------------
     * 成员变量
     *----------------------------------------------------*/
    std::function<void(void)> become_master_callback_;
    std::function<void(void)> become_slave_callback_;
    int                    node_id_;
    fs::path               pid_dir_;
    fs::path               lock_path_;
    fs::path               pid_file_;
    int                    lock_fd_ = -1;

    std::atomic<bool>      running_{false};
    std::atomic<bool>      is_master_{false};

    std::thread            heartbeat_thread_;
    std::thread            check_thread_;

    std::mutex             pids_mtx_;
    std::set<int>          slave_pids_;
};
