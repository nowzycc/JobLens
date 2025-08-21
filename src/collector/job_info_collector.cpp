#include "collector/job_info_collector.hpp"
#include <sstream>
#include "common/config.hpp"

#include <iostream>

const char* COLLECTOR_TYPE_PROC = "ProcCollector";

JobInfoCollector::JobInfoCollector()
{
    job_adder_.emplace(StreamWatcher::Config{
        .type = StreamWatcher::Type::FIFO,
        .path = Config::instance().getString("collectors_config", "job_adder_fifo")
    },
    [this](const char* buf, std::size_t len) {
        Job job;
        string2Job(std::string(buf, len), job);
        addJob(job);
    });

    registerCollectFuncs();
    registerFinishCallbacks();
    std::cout<<"registerFinishCallbacks done"<<std::endl;
    spdlog::info("JobInfoCollector: initialized with {} collect functions and {} finish callbacks",
                collectFuncs_.size(), finishCallbacks_.size());
}

JobInfoCollector::~JobInfoCollector() { shutdown(); }

void JobInfoCollector::addCollectFunc(std::string name, std::function<std::any(Job&)> f) {
    std::lock_guard lg(m_);
    collectFuncs_.push_back(std::make_tuple(std::move(name), std::move(f)));
}

void JobInfoCollector::addCallback(OnFinish cb) {
    std::lock_guard lg(m_);
    finishCallbacks_.push_back(std::move(cb));
}

void JobInfoCollector::addJob(Job job) {
    std::lock_guard lg(m_);
    currJobs_.push_back(job);
}

void JobInfoCollector::delJob(int jobID) {
    std::lock_guard lg(m_);
    auto it = std::remove_if(currJobs_.begin(), currJobs_.end(),
                             [=](const Job& j){ return j.JobID == jobID; });
    currJobs_.erase(it, currJobs_.end());
}

void JobInfoCollector::start() {
    std::lock_guard lg(m_);
    if (running_) return;
    running_ = true;
    for (const auto& func_tuple: collectFuncs_) {
        auto name = std::get<0>(func_tuple);
        timerScheduler_.registerRepeatingTimer(
            std::chrono::milliseconds(global_config.getInt(name, "freq")),
            [this, func_tuple]() {
                std::lock_guard lg(m_);
                auto name = std::get<0>(func_tuple);
                auto func = std::get<1>(func_tuple);
                for (auto& job : currJobs_) {
                    auto info = func(job);
                    for (const auto& cb : finishCallbacks_) {
                        cb(name, job, std::move(info), std::chrono::system_clock::now());
                    }
                    
                }
            }
        );
    }
}

void JobInfoCollector::shutdown() {
    {
        std::lock_guard lg(m_);
        if (!running_) return;
        running_ = false;
    }
    cv_.notify_all();
    for (auto& t : threads_) if (t.joinable()) t.join();
    threads_.clear();
}

nlohmann::json JobInfoCollector::snapshot() {
    nlohmann::json result;
    std::lock_guard lg(m_);
    return result;
}

JobInfoCollector& JobInfoCollector::instance() {
    static JobInfoCollector instance;
    return instance;
}

void JobInfoCollector::string2Job(const std::string& str, Job& job) {
    nlohmann::json j = nlohmann::json::parse(str);
    job.JobID = j["JobID"].get<int>();
    job.JobPIDs = j["JobPIDs"].get<std::vector<int>>();
    date::sys_seconds tp;
    std::istringstream in{j["JobCreateTime"].get<std::string>()};
    in >> date::parse("%F %T", tp);
    job.JobCreateTime = tp;
}

void JobInfoCollector::registerCollectFuncs() {
    global_config = Config::instance();
    struct Collector {
        std::string name;
        std::string type;
        std::string config;
    };
    
    auto collectors = global_config.getArray<Collector>("collectors_config", "collectors",
        [](const YAML::Node& node) {
            Collector c;
            c.name = node["name"].as<std::string>();
            c.type = node["type"].as<std::string>();
            c.config = node["config"].as<std::string>();
            return c;
        });

    for (const auto& collector : collectors) {
        if (collector.type == COLLECTOR_TYPE_PROC) {
            auto t = std::make_tuple(
                collector.name,
                proc_collector::collect
            );
            collectFuncs_.emplace_back(std::move(t));
        }
    }
}

void JobInfoCollector::registerFinishCallbacks() {
    auto callbacks = writer_manager::instance().get_onFinishCallbacks();
    std::lock_guard lg(m_);
    for (const auto& cb_func : callbacks) {
        addCallback(cb_func);
    }
}