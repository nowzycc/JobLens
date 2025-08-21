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


// ================================================================
class JobInfoCollector {
public:
    JobInfoCollector()
    {
        job_adder_.emplace(StreamWatcher::Config{
            .type = StreamWatcher::Type::FIFO,
            .path = Config::instance().getString("collector", "job_adder_fifo")
        },
        [this](const char* buf, std::size_t len) {
            Job job;
            string2Job(std::string(buf, len), job);
            addJob(job);
        });

        registerCollectFuncs();
        registerFinishCallbacks();
        
    };
    ~JobInfoCollector() { shutdown(); }


    // 非拷贝、可移动
    JobInfoCollector(const JobInfoCollector&)            = delete;
    JobInfoCollector& operator=(const JobInfoCollector&) = delete;
    JobInfoCollector(JobInfoCollector&&)                 = default;
    JobInfoCollector& operator=(JobInfoCollector&&)      = default;


    // 注册/注销
    void addCollectFunc(std::string name, std::function<void*(const Job&)> f) {
        std::lock_guard lg(m_);
        collectFuncs_.push_back(std::move(std::make_tuple(std::move(name), std::move(f))));
    }

    void addCallback(OnFinish cb) {
        std::lock_guard lg(m_);
        finishCallbacks_.push_back(std::move(cb));
    }
    
    void addJob(const Job& job) {
        std::lock_guard lg(m_);
        currJobs_.push_back(job);
    }

    void delJob(int jobID) {
        std::lock_guard lg(m_);
        auto it = std::remove_if(currJobs_.begin(), currJobs_.end(),
                                 [=](const Job& j){ return j.JobID == jobID; });
        currJobs_.erase(it, currJobs_.end());
    }

    // 启动采集循环
    void start() {
        std::lock_guard lg(m_);
        if (running_) return;
        running_ = true;
        //TODO：这里还可以按照任务展开，但是目前的架构耦合太深不方便改
        for (const auto& func_tuple: collectFuncs_) {
            auto name = std::get<0>(func_tuple);
            timerScheduler_.registerRepeatingTimer(
                std::chrono::milliseconds(global_config.getInt(name, "freq")),
                [this, func_tuple]() {
                    std::lock_guard lg(m_);
                    auto name = std::get<0>(func_tuple);
                    auto func = std::get<1>(func_tuple);
                    for (const auto& job : currJobs_) {
                        void* info = func(job);
                        if(info){
                            for (const auto& cb : finishCallbacks_) {
                                cb(name, job, info, std::chrono::system_clock::now());
                            }
                        }
                    }
                }
            );
        }
        
    }

    // 停止并等待
    void shutdown() {
        {
            std::lock_guard lg(m_);
            if (!running_) return;
            running_ = false;
        }
        cv_.notify_all();
        for (auto& t : threads_) if (t.joinable()) t.join();
        threads_.clear();
    }

    nlohmann::json snapshot() {
        
        nlohmann::json result;
        std::lock_guard lg(m_);
        // TODO: 实现类的全局快照
        return result;
    }

    // 获取单例实例
    static JobInfoCollector& instance() {
        static JobInfoCollector instance = JobInfoCollector();
        return instance;
    }
private:

    void string2Job(const std::string& str, Job& job) {
        nlohmann::json j = nlohmann::json::parse(str);
        job.JobID = j["JobID"].get<int>();
        job.JobPIDs = j["JobPIDs"].get<std::vector<int>>();
        date::sys_seconds tp;
        std::istringstream in{j["JobCreateTime"].get<std::string>()};
        in >> date::parse("%F %T", tp);
        job.JobCreateTime = tp;
    }

    void registerCollectFuncs() {
        global_config = Config::instance();
        auto func_names = global_config.getArray<std::string>("collector", "funcs");
        for (const auto& name : func_names) {
            if (name == "proc_collector") {
                
            }
        }
    }

    void registerFinishCallbacks() {
        auto callbacks = writer_manager::instance().get_onFinishCallbacks();
        std::lock_guard lg(m_);
        for (const auto& cb_func : callbacks) {
            addCallback(cb_func);
        }
    }

    
    Config global_config = Config::instance();
    TimerScheduler timerScheduler_ = TimerScheduler(Config::instance().getInt("collector", "max_collector_threads")); 
    std::optional<StreamWatcher> job_adder_;
    std::optional<StreamWatcher> job_remover_;
    std::mutex m_;
    std::vector<std::thread> threads_;
    std::vector<std::tuple<std::string, std::function<void*(const Job&)>>> collectFuncs_;
    std::vector<OnFinish> finishCallbacks_;
    std::vector<Job> currJobs_;
    bool running_ = false;
    std::condition_variable cv_;
};
