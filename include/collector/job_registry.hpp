// job_registry.h
#pragma once
#include <unordered_map>
#include <shared_mutex>
#include <vector>
#include <functional>
#include "collector/collector_type.h"
#include "job_lifecycle_event.h"



class JobRegistry {
public:
    static JobRegistry& instance();          // 仍保留单例，方便迁移；也可由 main() 构造
    ~JobRegistry() = default;

    // 禁止拷贝
    JobRegistry(const JobRegistry&)            = delete;
    JobRegistry& operator=(const JobRegistry&) = delete;

    // Job 增删
    void addJob(Job job);
    void delJob(int jobID);
    const Job* findJob(int jobID) const;
    std::vector<Job> snapshot() const;

    // 生命周期回调注册
    void addLifecycleCb(JobLifecycleCb cb);

private:
    JobRegistry();
    std::optional<StreamWatcher> job_opt_;
    mutable std::shared_mutex              mtx_;
    std::unordered_map<int, Job>           jobs_;   // key = JobID
    std::vector<JobLifecycleCb>            cbs_;
};