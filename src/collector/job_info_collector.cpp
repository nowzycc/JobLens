#include "collector/job_info_collector.hpp"
#include <sstream>
#include "common/config.hpp"

#include <iostream>

std::string string2JobOpt(const std::string& str, Job& job) {


    nlohmann::json j = nlohmann::json::parse(str);
    auto opt = j["opt"].get<std::string>();
    job.JobID = j["JobID"].get<int>();
    job.JobPIDs = j["JobPIDs"].get<std::vector<int>>();
    if (job.JobPIDs.size() == 0) {
        spdlog::warn("JobInfoCollector: job ID {} has empty PID list", job.JobID);
    }
    for (auto pid: job.JobPIDs) {
        if (pid > 0) {
            spdlog::warn("JobInfoCollector: invalid PID {} in job ID {}", pid, job.JobID);
        }
    }
    try
    {
        date::sys_seconds tp;
        std::istringstream in{j["JobCreateTime"].get<std::string>()};
        in >> date::parse("%F %T", tp);
        job.JobCreateTime = tp;
    }
    catch(const std::exception& e)
    {
        spdlog::warn("JobInfoCollector: error parsing JobCreateTime for job ID {}: {}", job.JobID, e.what());
        // job.JobCreateTime = std::chrono::system_clock::now();
    }
    return opt;
    
}


JobInfoCollector::JobInfoCollector()
{
    job_opt_.emplace(StreamWatcher::Config{
        .type = StreamWatcher::Type::FIFO,
        .path = Config::instance().getString("collectors_config", "job_adder_fifo")
    },
    [this](const char* buf, std::size_t len) {
        Job job;
        auto opt = string2JobOpt(std::string(buf, len), job);
        if(opt.compare("add") == 0){

        }
        if(opt.compare("remove") == 0){
            
        }
    });
    

    registerCollectFuncs();
    registerFinishCallbacks();
    spdlog::info("JobInfoCollector: initialized with {} collect functions and {} finish callbacks",
                collectFuncs_.size(), finishCallbacks_.size());
}

JobInfoCollector::~JobInfoCollector() { shutdown(); }

void JobInfoCollector::initCollector(){
    
}

void JobInfoCollector::addJob2Collector(Job& job, std::string collector){
    auto state = collector_job_list[collector];
    state.job_list.push_back(job);
    if(!state.running){
        
    }
}

void JobInfoCollector::addJobCollect(Job& job){
    std::lock_guard lg(m_);
    for(auto collector_name:job.CollectorNames){
        addJob2Collector(job, collector_name);
    }
}

void JobInfoCollector::addCollectFunc(std::string name, std::string config, std::function<std::any(Job&)> f) {
    std::lock_guard lg(m_);
    collectFuncs_.push_back(std::make_tuple(name, config, std::move(f)));
}

void JobInfoCollector::addCallback(OnFinish cb) {
    std::lock_guard lg(m_);
    finishCallbacks_.push_back(std::move(cb));
}

void JobInfoCollector::onJobLifecycle(JobEvent ev, Job& job) {
    switch (ev) {
        case JobEvent::Added: {
            spdlog::info("JobInfoCollector: job {} added, {} PIDs", job.JobID, job.JobPIDs.size());
            addJobCollect(job);
            break;
        }
        case JobEvent::Removed: {
            spdlog::info("JobInfoCollector: job {} removed", job.JobID);
            break;
        }
        default: {
            spdlog::warn("JobInfoCollector: error job event");
        }
    }
}

void JobInfoCollector::start() {
    std::lock_guard lg(m_);
    if (running_) return;
    running_ = true;
    job_opt_->start(); //开始接收任务添加
}

void JobInfoCollector::shutdown() {
    {
        std::lock_guard lg(m_);
        if (!running_) return;
        running_ = false;
    }
    job_opt_->stop();
    timerScheduler_.shutdown();
    writer_manager::instance().shutdown();
    spdlog::info("JobInfoCollector: writer_manager shutdown complete");
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
            addCollectFunc(
                collector.name,
                collector.config,
                proc_collector::collect
            );
        }
    }
}

void JobInfoCollector::registerFinishCallbacks() {
    auto callbacks = writer_manager::instance().get_onFinishCallbacks();
    
    for (const auto& cb_func : callbacks) {
        addCallback(cb_func);
    }
}