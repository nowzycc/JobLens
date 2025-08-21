#include "collector/job_info_collector.hpp"
#include <sstream>

JobInfoCollector::JobInfoCollector()
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
}

JobInfoCollector::~JobInfoCollector() { shutdown(); }

void JobInfoCollector::addCollectFunc(std::string name, std::function<void*(const Job&)> f) {
    std::lock_guard lg(m_);
    collectFuncs_.push_back(std::make_tuple(std::move(name), std::move(f)));
}

void JobInfoCollector::addCallback(OnFinish cb) {
    std::lock_guard lg(m_);
    finishCallbacks_.push_back(std::move(cb));
}

void JobInfoCollector::addJob(const Job& job) {
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
    auto func_names = global_config.getArray<std::string>("collector", "funcs");
    for (const auto& name : func_names) {
        if (name == "proc_collector") {
            // 原实现为空
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