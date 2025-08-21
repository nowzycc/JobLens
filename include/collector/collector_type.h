#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <any>

enum class CollectorType {
    ProcColletor,      // 采集 /proc/<pid>/stat
    kStatus,    // 采集 /proc/<pid>/status
    kCmdline,   // 采集 /proc/<pid>/cmdline
    kFd         // 采集 /proc/<pid>/fd 信息
};

struct Job {
    int                                     JobID{};
    std::vector<int>                        JobPIDs;
    std::chrono::system_clock::time_point   JobCreateTime{std::chrono::system_clock::now()};
    std::unordered_map<std::string, void*> JobInfo; // 用 json 存任意值
};

using OnFinish = std::function<void(const std::string, const Job&, const std::any, std::chrono::system_clock::time_point)>;