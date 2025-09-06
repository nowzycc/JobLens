// job_registry.cpp
#include "collector/job_registry.hpp"
#include <spdlog/spdlog.h>
#include "common/streamer_watcher.hpp"
#include <date/date.h>
#include "common/config.hpp"
#include <signal.h>

std::string string2JobOpt(const std::string& str, Job& job) {


    nlohmann::json j = nlohmann::json::parse(str);
    auto opt = j["opt"].get<std::string>();
    job.JobID = j["JobID"].get<int>();
    job.JobPIDs = j["JobPIDs"].get<std::vector<int>>();
    job.CollectorNames = j["Lens"].get<std::vector<std::string>>();
    if (job.JobPIDs.size() == 0) {
        spdlog::warn("JobRegistry: job ID {} has empty PID list", job.JobID);
    }
    for (auto pid: job.JobPIDs) {
        if (pid > 0) {
            spdlog::warn("JobRegistry: invalid PID {} in job ID {}", pid, job.JobID);
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
        spdlog::warn("JobRegistry: error parsing JobCreateTime for job ID {}: {}", job.JobID, e.what());
    }
    return opt;
    
}


JobRegistry::JobRegistry(){
        job_opt_.emplace(StreamWatcher::Config{
            .type = StreamWatcher::Type::FIFO,
            .path = Config::instance().getString("collectors_config", "job_adder_fifo")
            },
            [this](const char* buf, std::size_t len) {
                Job job;
                std::string opt;
                try
                {
                    opt = string2JobOpt(std::string(buf, len), job);
                }
                catch(const std::exception& e)
                {
                    spdlog::error("JobRegistry: job_opt parse error: {}",e.what());
                    return;
                }
                
                if(opt.compare("add") == 0){
                    addJob(job);
                }
                if(opt.compare("remove") == 0){
                    delJob(job.JobID);
                }
        });
        job_opt_->start();
    };

JobRegistry& JobRegistry::instance() {
    static JobRegistry reg;
    return reg;
}

void JobRegistry::addJob(Job job) {
    {
        std::unique_lock lg(mtx_);
        if (jobs_.count(job.JobID)) {
            spdlog::warn("JobRegistry: duplicate jobID {}, ignored", job.JobID);
            return;
        }
        jobs_.emplace(job.JobID, std::move(job));
    }
    for (const auto& cb : cbs_) cb(JobEvent::Added, jobs_.at(job.JobID));
    spdlog::info("JobRegistry: add job with JobID {}", job.JobID);
}

void JobRegistry::delJob(int jobID) {
    Job removed;                // 先拷贝出来，再回调，避免锁内回调
    {
        std::unique_lock lg(mtx_);
        auto it = jobs_.find(jobID);
        if (it == jobs_.end()) return;
        removed = std::move(it->second);
        jobs_.erase(it);
    }
    for (const auto& cb : cbs_) cb(JobEvent::Removed, removed);
    spdlog::info("JobRegistry: remove job with JobID {}, JobPIDs {}", removed.JobID);
}

inline bool is_process_running(pid_t pid) {
    return kill(pid, 0) == 0;
}

const Job* JobRegistry::findJob(int jobID) const
{
    std::vector<int> toDelete;          
    const Job* ret = nullptr;

    {
        std::shared_lock lg(mtx_);
        auto it = jobs_.find(jobID);
        if (it == jobs_.end()) return nullptr;

        Job job = it->second;  

        // 过滤 PID
        job.JobPIDs.erase(
            std::remove_if(job.JobPIDs.begin(),
                           job.JobPIDs.end(),
                           [](pid_t pid){ return !is_process_running(pid); }),
            job.JobPIDs.end());

        if (job.JobPIDs.empty()) {
            toDelete.push_back(jobID);   
            spdlog::info("JobRegistry: job {} has no running process, delete it", job.JobID);
        }
        ret = &it->second;
    }  

    for (int id : toDelete)
        const_cast<JobRegistry*>(this)->delJob(id);   // 非 const 调用

    return toDelete.empty() ? ret : nullptr;   // 如果删了，就返回空
}


std::vector<Job> JobRegistry::snapshot() const {
    std::shared_lock lg(mtx_);
    std::vector<Job> out;
    out.reserve(jobs_.size());
    for (const auto& [id, job] : jobs_) out.push_back(job);
    return out;
}

void JobRegistry::addLifecycleCb(JobLifecycleCb cb) {
    std::unique_lock lg(mtx_);
    cbs_.push_back(std::move(cb));
}