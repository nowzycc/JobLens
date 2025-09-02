// job_registry.cpp
#include "collector/job_registry.hpp"
#include <spdlog/spdlog.h>

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
}

const Job* JobRegistry::findJob(int jobID) const {
    std::shared_lock lg(mtx_);
    auto it = jobs_.find(jobID);
    return it == jobs_.end() ? nullptr : &it->second;
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