#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <functional>
#include <any>
#include <nlohmann/json.hpp>

#define COLLECTOR_TYPE_PROC "ProcCollector"

enum class CollectorType {
    ProcCollector,      // 采集 /proc/<pid>/stat
    kStatus,    // 采集 /proc/<pid>/status
    kCmdline,   // 采集 /proc/<pid>/cmdline
    kFd         // 采集 /proc/<pid>/fd 信息
};

struct Job {
    int                                     JobID{};
    std::vector<int>                        JobPIDs;
    std::chrono::system_clock::time_point   JobCreateTime{std::chrono::system_clock::now()};
    std::unordered_map<std::string, void*>  JobInfo;
    std::vector<std::string>                CollectorNames;
};

using OnFinish = std::function<void(const std::string, const Job&, const std::any, std::chrono::system_clock::time_point)>;

using CollectResult = std::any;

// 统一的可调用签名
using CollectFunc = std::function<CollectResult(const Job&)>;
using CollectInitFunc = std::function<bool(const nlohmann::json& config)>;
using CollectDeinitFunc = std::function<void()>;


struct CollectorHandle {
    CollectInitFunc        init;   
    CollectFunc            collect;
    CollectDeinitFunc      deinit;
};
