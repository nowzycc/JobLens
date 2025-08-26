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
#include <any>
// 前置声明，降低头文件耦合
class Job;

namespace proc_collector {

struct proc_info {
    int8_t      type{int8_t(CollectorType::ProcColletor)};
    int         pid{};
    std::string name;
    int         ppid{};
    double      cpuPercent{};      // CPU 占用率
    std::size_t memoryRss{};       // 字节
    double      memoryPercent{};
    int         numThreads{};
    int         ioReadCount{};
    int         ioWriteCount{};
    int         netConnCount{};
    std::string status{"unknown"};
};

/* 获取系统总内存（单位：KB），失败返回 nullopt */
std::optional<std::size_t> getMemTotalKb();

/* 对单个进程采集一次快照 */
std::unique_ptr<proc_info> snapshotOf(int pid);

/* 根据 Job 对象采集所有目标进程 */
std::any collect(Job& job);

} // namespace proc_collector