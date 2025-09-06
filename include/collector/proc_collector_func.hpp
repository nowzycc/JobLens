#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LOGGER_TRACE

#include <fstream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>
#include <optional>
#include <fmt/core.h>
#include <memory>
#include "collector/collector_type.h"
#include "icollector.h"
#include <any>
// 前置声明，降低头文件耦合
class Job;

namespace proc_collector {

struct proc_info {
    int8_t      type{int8_t(CollectorType::ProcCollector)};
    int         pid{};
    std::string name;
    int         ppid{};
    // CPU 相关信息
    double      cpuPercent{};      
    unsigned long long utime{};
    unsigned long long stime{};
    unsigned long long starttime{};
    long hz{};
    long numCores{};
    
    // 内存相关信息
    std::size_t memoryRss{};       // 字节
    double      memoryPercent{};
    int         numThreads{};
    int         ioReadCount{};
    int         ioWriteCount{};
    int         netConnCount{};
    std::string status{"unknown"};
};

class ProcCollector : public ICollector {
public:
    bool init(const nlohmann::json& cfg) override;
    CollectResult collect(const Job& job) override;
    void deinit() noexcept override;
private:
    std::any impl_collect(const Job& job);
    std::unique_ptr<proc_info> snapshotOf(int pid);

    struct pid_state{
        unsigned long long lastTotal{};
        unsigned long long lastProc{};
    };

    std::unordered_map<int, pid_state> pid_state_dict;
};


} // namespace proc_collector