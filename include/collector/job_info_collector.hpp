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
    void addCollectFunc(std::string name, std::string config, CollectFunc colloctor_handle,CollectInitFunc init_handle,CollectDeinitFunc deinit_handle);
    void addCallback(OnFinish cb);
    void start();
    void shutdown();
    
    nlohmann::json snapshot();

    static JobInfoCollector& instance();

private:
    void registerCollectFuncs();
    void registerFinishCallbacks();
    void onJobLifecycle(JobEvent ev, Job& job);
    void addJobCollect(Job& job);
    void startCollector(std::string collector);
    void addJob2Collector(Job& job, std::string collector);
    Config& global_config = Config::instance();
    TimerScheduler timerScheduler_;
    std::optional<StreamWatcher> job_opt_;

    struct collector_state{
        std::vector<std::reference_wrapper<Job>> job_list;
        std::mutex              m_;
        size_t task_id;
        bool running;
    };

    struct collector_info
    {
        std::string name;
        std::string config_name;
        CollectFunc collect_handle;
        CollectInitFunc init_handle;
        CollectDeinitFunc deinit_handle;
    };
    

    std::mutex              m_;
    std::unordered_map<std::string, collector_info> collector_info_dict;
    std::unordered_map<std::string, collector_state> collector_job_dict;
    std::vector<OnFinish>   finishCallbacks_;
    bool                    running_ = false;
};