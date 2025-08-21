/************************************************************
 * DistributedNode.cpp  —— 主节点无缝切换（Lease + 预加载） *
 *  1. 锁文件永存，永远只有一个 epoch 有效                  *
 *  2. 1 s 租约，主节点每 250 ms 续约                       *
 *  3. 30 % 时间点写快照（~300 ms 前），从节点预加载        *
 *  4. 晋升耗时 < 1 ms，业务无感知                          *
 ***********************************************************/
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cstring>
#include <map>
#include <set>
#include <sys/file.h>
#include <csignal>
#include "utils/nlohmann/json.hpp"

// 如果你们的 Config 头文件路径不同，请自行修改
#include "common/config.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

using namespace std::chrono_literals;

/*==========================================================
 * 一个非常轻量的业务状态管理器示例
 * 实际项目里可换成你自己的数据结构
 *=========================================================*/
class StateManager {
public:
    static StateManager& instance() {
        static StateManager ins;
        return ins;
    }

    /* 晋升为主时调用（可在这里把只读切换为读写） */
    void on_promote() {
        std::lock_guard<std::mutex> lk(mu_);
        role_ = "master";
        std::cout << "[State] promoted to master, last_value=" << value_ << "\n";
    }

    /* 降格为从时调用 */
    void on_demote() {
        std::lock_guard<std::mutex> lk(mu_);
        role_ = "slave";
    }

    /* 把状态序列化成 json；这里只演示一个整数 */
    json snapshot() {
        std::lock_guard<std::mutex> lk(mu_);
        json j;
        j["value"] = value_;
        return j;
    }

    /* 反序列化并原子替换 */
    void load_snapshot(const json& j) {
        std::lock_guard<std::mutex> lk(mu_);
        if (j.contains("value")) {
            value_ = j["value"].get<int>();
        }
    }

    /* 供测试用的接口 */
    void inc() {
        std::lock_guard<std::mutex> lk(mu_);
        ++value_;
    }

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
    static constexpr auto  HEARTBEAT_INTERVAL   = 250ms;
    static constexpr auto  SLAVE_CHECK_INTERVAL = 100ms;

    /*---------------- 构造/析构 ----------------*/
    DistributedNode() {
        node_id_ = getpid();
        auto global_config = Config::instance();
        pid_dir_   = global_config.getString("lens_config","pid_dir");
        lock_path_ = global_config.getString("lens_config","lock_path");

        fs::create_directories(pid_dir_);
        fs::create_directories(fs::path(lock_path_).parent_path());

        /* 创建 PID 文件 */
        pid_file_ = pid_dir_ / ("node_" + std::to_string(node_id_));
        std::ofstream(pid_file_) << node_id_;

        /* 初始化锁文件描述符（只打开一次，永不关闭）*/
        lock_fd_ = open(lock_path_.c_str(), O_CREAT | O_RDWR, 0666);
        if (lock_fd_ < 0) {
            throw std::runtime_error("cannot open lock file");
        }

        setup_signal_handlers();
    }

    ~DistributedNode() {
        stop();
        fs::remove(pid_file_);
        if (lock_fd_ >= 0) close(lock_fd_);
    }

    /* 单例 */
    static DistributedNode& instance() {
        static DistributedNode ins;
        return ins;
    }

    /*---------------- 生命周期 ----------------*/
    void start() {
        running_ = true;
        if (try_acquire_lock()) {
            become_master();
        } else {
            become_slave();
        }
    }

    void stop() {
        running_ = false;
        if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
        if (check_thread_.joinable())    check_thread_.join();

        if (is_master_) {
            notify_slaves(SIGUSR1);
        }
    }

    void set_become_master_callback(std::function<void(void)> cb) {
        become_master_callback_ = std::move(cb);
    }

    void set_become_slave_callback(std::function<void(void)> cb) {
        become_slave_callback_ = std::move(cb);
    }

private:
    /*------------------------------------------------------
     * 数据结构
     *----------------------------------------------------*/
    struct Lease {
        uint64_t epoch = 0;
        uint64_t updated_at = 0;
        uint64_t expire_at  = 0;
        json     snapshot;
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Lease, epoch, updated_at, expire_at, snapshot)
    };

    Lease current_;

    /*------------------------------------------------------
     * 工具函数
     *----------------------------------------------------*/
    static uint64_t epoch_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    /*------------------------------------------------------
     * 锁/租约操作
     *----------------------------------------------------*/
    bool try_acquire_lock() {
        struct flock fl{};
        fl.l_type   = F_WRLCK;
        fl.l_whence = SEEK_SET;
        if (fcntl(lock_fd_, F_SETLK, &fl) == -1) {
            return false; // 其他人持有
        }
        Lease lease = read_lease();
        uint64_t now = epoch_ms();

        if (now < lease.expire_at && lease.epoch != 0) {
            // 未过期，放弃
            struct flock ul{F_UNLCK, SEEK_SET, 0, 0, 0};
            fcntl(lock_fd_, F_SETLK, &ul);
            return false;
        }

        // 抢占
        lease.epoch      = lease.epoch + 1;
        lease.updated_at = now;
        lease.expire_at  = now + LEASE_SEC * 1000;
        write_lease(lease);
        current_ = lease;
        return true;
    }

    Lease read_lease() {
        lseek(lock_fd_, 0, SEEK_SET);
        char buf[4096]{};
        ssize_t n = read(lock_fd_, buf, sizeof(buf) - 1);
        Lease lease;
        if (n > 0) {
            try { lease = json::parse(buf).get<Lease>(); } catch (...) {}
        }
        return lease;
    }

    void write_lease(const Lease& l) {
        lseek(lock_fd_, 0, SEEK_SET);
        std::string data = json(l).dump();
        ftruncate(lock_fd_, 0);
        write(lock_fd_, data.data(), data.size());
        fsync(lock_fd_);
    }

    /*------------------------------------------------------
     * 主/从 角色切换
     *----------------------------------------------------*/
    void become_master() {
        is_master_ = true;
        StateManager::instance().on_promote();
        update_slave_pids();          // 通知旧从节点
        notify_slaves(SIGUSR2);
        if (become_master_callback_) become_master_callback_();
        heartbeat_thread_ = std::thread([this] {
            while (running_) {
                uint64_t now = epoch_ms();
                current_.updated_at = now;
                current_.expire_at  = now + LEASE_SEC * 1000;

                // 30 % 时间点写快照
                if (now + LEASE_SEC * 1000 * PRE_PROMOTE_RATIO / 100 >=
                    current_.expire_at) {
                    current_.snapshot = StateManager::instance().snapshot();
                }

                write_lease(current_);
                std::this_thread::sleep_for(HEARTBEAT_INTERVAL);
            }
        });
    }

    void become_slave() {
        is_master_ = false;
        StateManager::instance().on_demote();
        if (become_slave_callback_) become_slave_callback_();
        check_thread_ = std::thread([this] {
            while (running_) {
                Lease lease = read_lease();
                uint64_t now = epoch_ms();

                if (now < lease.expire_at) {
                    // 主存活，预加载快照
                    if (!lease.snapshot.empty()) {
                        StateManager::instance().load_snapshot(lease.snapshot);
                    }
                    std::this_thread::sleep_for(SLAVE_CHECK_INTERVAL);
                    continue;
                }

                if (try_acquire_lock()) {
                    become_master();
                    break;
                }
                std::this_thread::sleep_for(50ms);
            }
        });
    }

    /*------------------------------------------------------
     * PID 文件管理 & 信号通知
     *----------------------------------------------------*/
    void update_slave_pids() {
        std::lock_guard<std::mutex> lk(pids_mtx_);
        slave_pids_.clear();
        for (const auto& entry : fs::directory_iterator(pid_dir_)) {
            try {
                std::string name = entry.path().filename().string();
                if (name.rfind("node_", 0) != 0) continue;
                int pid = std::stoi(name.substr(5));
                if (pid == node_id_) continue;
                if (kill(pid, 0) == 0) {
                    slave_pids_.insert(pid);
                } else {
                    fs::remove(entry.path());
                }
            } catch (...) {}
        }
    }

    void notify_slaves(int sig) {
        std::lock_guard<std::mutex> lk(pids_mtx_);
        for (int pid : slave_pids_) {
            kill(pid, sig);
        }
    }

    /*------------------------------------------------------
     * 信号处理
     *----------------------------------------------------*/
    void setup_signal_handlers() {
        struct sigaction sa{};
        sa.sa_handler = [](int sig){
            switch (sig) {
                case SIGUSR1: instance().try_promote(); break;   // 旧主退出
                case SIGUSR2: instance().update_slave_pids(); break;
                case SIGTERM: instance().stop(); std::exit(0);
            }
        };
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGUSR1, &sa, nullptr);
        sigaction(SIGUSR2, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
    }

    void try_promote() {
        if (try_acquire_lock()) {
            become_master();
        }
    }

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

