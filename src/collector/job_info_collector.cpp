#include "collector/job_info_collector.hpp"
#include "collector/collector_registry.hpp"
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
                collector_info_dict.size(), collector_info_dict.size());
}

JobInfoCollector::~JobInfoCollector() { shutdown(); }

void JobInfoCollector::startCollector(std::string collector_name){
    std::function<std::any(Job&)> func_handle;
    collector_info info; 
    std::string config_name;
    try
    {
        info = collector_info_dict.at(collector_name);
    }
    catch(const std::exception& e)
    {
        spdlog::error("JobInfoCollector: start collector error, can not find {}!",collector_name);
        return;
    }
    auto config_node = Config::instance().getRawNode(info.config_name);
    auto j_config = yamlToJson(config_node);
    info.init_handle(j_config);
    auto freq = Config::instance().getInt(info.config_name, "freq");

    auto& collector_job = collector_job_dict[collector_name];

    
    collector_job.task_id = timerScheduler_.registerRepeatingTimer(
        std::chrono::milliseconds(1000/freq),
        [this, collector_name](){
            auto& info = collector_info_dict[collector_name];
            auto& collector_job = collector_job_dict[collector_name];
            {
                std::lock_guard lg(collector_job.m_);
                if(collector_job.job_list.size() == 0){
                    //没有任务，取消这个收集器，节省资源
                    timerScheduler_.cancelTimer(collector_job.task_id);
                }
                for(auto& job:collector_job.job_list){
                    //TODO:当压力过高时，这里应该改为非阻塞执行
                    auto ret = info.collect_handle(job);
                    for(auto& cb:finishCallbacks_){
                        cb(collector_name, job, ret, std::chrono::system_clock::now());
                    }
                }
            }
        }
    );

}

void JobInfoCollector::addJob2Collector(Job& job, std::string collector){
    auto& state = collector_job_dict[collector];
    {
        std::lock_guard lg(state.m_);
        state.job_list.push_back(job);
    }
    if(!state.running){
        startCollector(collector);
    }
}

void JobInfoCollector::addJobCollect(Job& job){
    std::lock_guard lg(m_);
    for(auto collector_name:job.CollectorNames){
        addJob2Collector(job, collector_name);
    }
}

void JobInfoCollector::addCollectFunc(std::string name, std::string config, CollectFunc collector_handle,CollectInitFunc init_handle,CollectDeinitFunc deinit_handle) {
    std::lock_guard lg(m_);
    collector_info_dict[name].name = name;
    collector_info_dict[name].config_name = config;
    collector_info_dict[name].collect_handle = collector_handle;
    collector_info_dict[name].init_handle = init_handle;
    collector_info_dict[name].deinit_handle = deinit_handle;
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

// 递归将 YAML 节点转换为 JSON
nlohmann::json yamlToJson(const YAML::Node& node) {
    if (node.IsNull()) {
        return nullptr;
    } else if (node.IsScalar()) {
        return node.as<std::string>(); // 可根据需要转换类型
    } else if (node.IsSequence()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& item : node) {
            arr.push_back(yamlToJson(item));
        }
        return arr;
    } else if (node.IsMap()) {
        nlohmann::json obj = nlohmann::json::object();
        for (const auto& kv : node) {
            obj[kv.first.as<std::string>()] = yamlToJson(kv.second);
        }
        return obj;
    }
    return nullptr;
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
    auto collector_reg = CollectorRegistry::instance();
    
    for (const auto& collector : collectors) {
        
        auto collector_handle = collector_reg.createCollector(collector.type);
        if (!collector_handle.init) {
            spdlog::error("JobInfoCollector: {} init error",collector.name);
        }
        addCollectFunc(
            collector.name,
            collector.config,
            collector_handle.collect,
            collector_handle.init,
            collector_handle.deinit
        );
    }
}

void JobInfoCollector::registerFinishCallbacks() {
    auto callbacks = writer_manager::instance().get_onFinishCallbacks();
    
    for (const auto& cb_func : callbacks) {
        addCallback(cb_func);
    }
}