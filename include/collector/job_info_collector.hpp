#pragma once
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>
#include <future>
#include <string>
#include <unordered_map>
#include <optional>
#include <fmt/core.h>
#include <fmt/chrono.h>
#include <date/date.h>

#include "collector/proc_collector_func.hpp"
#include "collector/collector_type.h"
#include "common/config.hpp"
#include "common/streamer_watcher.hpp"
#include "common/timer_scheduler.hpp"
#include "writer/writer_manager.hpp"
#include "utils/nlohmann/json.hpp"

class JobInfoCollector {
public:
    JobInfoCollector();
    ~JobInfoCollector();

    // 非拷贝、可移动
    JobInfoCollector(const JobInfoCollector&)            = delete;
    JobInfoCollector& operator=(const JobInfoCollector&) = delete;
    JobInfoCollector(JobInfoCollector&&)                 = default;
    JobInfoCollector& operator=(JobInfoCollector&&)      = default;

    // 对外接口
    void addCollectFunc(std::string name, std::function<std::any(Job&)> f);
    void addCallback(OnFinish cb);
    void addJob(Job job);
    void delJob(int jobID);
    void start();
    void shutdown();
    nlohmann::json snapshot();

    static JobInfoCollector& instance();

private:
    void string2Job(const std::string& str, Job& job);
    void registerCollectFuncs();
    void registerFinishCallbacks();

    Config& global_config = Config::instance();
    TimerScheduler timerScheduler_;
    std::optional<StreamWatcher> job_adder_;
    std::optional<StreamWatcher> job_remover_;

    std::mutex              m_;
    std::vector<std::thread> threads_;
    std::vector<std::tuple<std::string, std::function<std::any(Job&)>>> collectFuncs_;
    std::vector<OnFinish>   finishCallbacks_;
    std::vector<Job>        currJobs_;
    bool                    running_ = false;
    std::condition_variable cv_;
};