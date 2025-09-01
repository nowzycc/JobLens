#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LOGGER_TRACE

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

#include "job_lifecycle_event.h"

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
    void addCollectFunc(std::string name, std::string config, std::function<std::any(Job&)> f);
    void addCallback(OnFinish cb);
    void addJob(Job job);
    void delJob(int jobID);
    void start();
    void shutdown();
    nlohmann::json snapshot();

    static JobInfoCollector& instance();

private:
    void registerCollectFuncs();
    void registerFinishCallbacks();
    void onJobLifecycle(JobEvent ev, Job& job);
    void addJobCollect(Job& job);
    void initCollector();
    void addJob2Collector(Job& job, std::string collector);
    Config& global_config = Config::instance();
    TimerScheduler timerScheduler_;
    std::optional<StreamWatcher> job_opt_;
    
    struct collector_state{
        std::vector<Job&> job_list;
        bool running;
    };

    std::mutex              m_;
    std::vector<std::tuple<std::string, std::string, std::function<std::any(Job&)>>> collectFuncs_;
    std::unordered_map<std::string, collector_state> collector_job_list;
    std::vector<OnFinish>   finishCallbacks_;
    bool                    running_ = false;
};