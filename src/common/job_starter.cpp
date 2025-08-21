#include "common/job_starter.hpp"
#include <unistd.h>      // POSIX
#include <sys/wait.h>    // waitpid
#include <signal.h>      // kill
#include <stdexcept>
#include <spdlog/spdlog.h> // 仅用于日志，可替换

using namespace std::chrono_literals;

// ------------------------------------------------------------------
// 细节 1：回调注册/替换
void JobStarter::setCallback(OnExit cb) {
    std::lock_guard lg(mtx_);
    callback_ = std::move(cb);
}

// ------------------------------------------------------------------
// 细节 2：真正创建子进程
bool JobStarter::launch(const Options& opt) {
    if (opt.exe.empty()) return false;

    OnExit local_cb;
    {
        std::lock_guard lg(mtx_);
        if (!callback_) {
            spdlog::warn("JobStarter: no callback registered");
            return false;
        }
        local_cb = callback_;   // 复制，避免在子线程中持有锁
    }

    pid_t pid = fork();
    if (pid < 0) {               // fork 失败
        spdlog::error("JobStarter: fork failed");
        return false;
    }

    if (pid == 0) {              // ---------- 子进程 ----------
        setuid(getuid()); // 降低权限，避免 root 权限问题
        seteuid(getuid());
        // 组装 argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(opt.exe.c_str()));
        for (auto& s : opt.args) argv.push_back(const_cast<char*>(s.c_str()));
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        // execvp 只有失败才会返回
        spdlog::error("JobStarter: execvp failed for {}", opt.exe);
        _exit(127);              // 子进程立即退出
    }

    // ---------- 父进程 ----------
    spdlog::info("JobStarter: started child {}", pid);
    childPID = pid; 
    std::chrono::milliseconds tout = opt.timeout.value_or(0ms);
    std::thread th(&JobStarter::worker, this, pid, tout, std::move(local_cb));

    {
        std::lock_guard lg(mtx_);
        workers_.push_back(std::move(th));
    }
    return true;
}

// ------------------------------------------------------------------
// 细节 3：工作线程——等待子进程结束并调用回调
void JobStarter::worker(int pid,
                      std::chrono::milliseconds timeout,
                      OnExit cb)
{
    int status = 0;
    pid_t ret  = 0;

    if (timeout > 0ms) {
        // 使用 waitpid + 超时轮询
        auto deadline = std::chrono::steady_clock::now() + timeout;
        do {
            ret = waitpid(pid, &status, WNOHANG);
            if (ret == pid) break;
            if (shutdown_.load(std::memory_order_acquire)) {
                // 细节 4：收到全局关闭信号，强制杀进程
                ::kill(pid, SIGKILL);
                waitpid(pid, &status, 0);   // 回收
                return;
            }
            std::this_thread::sleep_for(10ms);
        } while (std::chrono::steady_clock::now() < deadline);

        if (ret == 0) { // 仍然没退出，超时
            ::kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            status = -1;   // 自定义超时码
            spdlog::warn("JobStarter: child {} killed on timeout", pid);
        }
    } else {
        // 永久等待
        do {
            ret = waitpid(pid, &status, 0);
        } while (ret == -1 && errno == EINTR);
    }

    int exit_code = 0;
    if (WIFEXITED(status))
        exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
        exit_code = 128 + WTERMSIG(status);
    else
        exit_code = -1;

    // 细节 5：回调一定在主线程之外执行，避免阻塞
    if (cb) cb(pid, exit_code);
}

// ------------------------------------------------------------------
// 细节 6：优雅关闭
void JobStarter::shutdown() {
    shutdown_.store(true, std::memory_order_release);
    std::unique_lock lg(mtx_);
    for (auto& th : workers_)
        if (th.joinable()) th.join();
    workers_.clear();
}