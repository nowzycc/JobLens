#pragma once
#include <fstream>
#include <sstream>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <vector>
#include <optional>
#include <fmt/core.h>
#include "collector/collector_type.h"


struct proc_info {
    int8_t type{int8_t(CollectorType::ProcColletor)}; // CollectorType
    int pid;
    std::string name;
    int         ppid{};
    double     cpuPercent{}; // CPU 占用率
    std::size_t memoryRss{};      // 字节
    double      memoryPercent{};
    int         numThreads{};
    int        ioReadCount{};    
    int        ioWriteCount{};
    int         netConnCount{};
    std::string status{"unknown"};
};




static std::optional<std::size_t> getMemTotalKb()
{
    std::ifstream f("/proc/meminfo");
    std::string line;
    while (std::getline(f, line)) {
        if (line.compare(0, 9, "MemTotal:") == 0) {
            std::size_t kb;
            std::istringstream iss(line);
            std::string _;
            iss >> _ >> kb;
            return kb;
        }
    }
    return std::nullopt;
}

std::unique_ptr<proc_info> snapshotOf(int pid)
{
    try {
        auto info = std::make_unique<proc_info>();


        /* 1. /proc/<pid>/stat ------------------------------------------------ */
        {
            std::ifstream f(fmt::format("/proc/{}/stat", pid));
            if (!f)
                return nullptr;

            std::string line;
            std::getline(f, line);
            std::istringstream iss(line);

            int _pid;
            iss >> _pid;
            if (iss.peek() == ' ') iss.ignore();   // 去掉空格
            if (iss.peek() == '(') {               // 进程名可能带空格，需特殊处理
                iss.ignore();
                std::getline(iss, info->name, ')');
            } else {
                iss >> info->name;
            }

            char state;
            iss >> state >> info->ppid;                // 第 2 列 state，第 3 列 ppid

            /* 跳到第 14 列（utime） */
            unsigned long long utime, stime;
            for (int i = 0; i < 11; ++i) iss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
            iss >> utime >> stime;                 // 14,15

            unsigned long long starttime;
            for (int i = 0; i < 4; ++i) iss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
            iss >> starttime;                      // 22
        }

        /* 2. /proc/<pid>/statm ------------------------------------------------- */
        {
            std::ifstream f(fmt::format("/proc/{}/statm", pid));
            if (f) {
                unsigned long vmsize, rss;
                f >> vmsize >> rss;
                info->memoryRss = rss * static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
            }
        }

        /* 3. /proc/<pid>/status ------------------------------------------------- */
        {
            std::ifstream f(fmt::format("/proc/{}/status", pid));
            if (!f)
                return nullptr;

            std::string line;
            static const auto get_kb = [](const std::string& l) -> std::size_t {
                std::istringstream iss(l);
                std::string key; std::size_t kb;
                iss >> key >> kb;
                return kb;
            };

            while (std::getline(f, line)) {
                if (line.compare(0, 6, "VmRSS:")) {
                    std::size_t kb = get_kb(line);
                    if (auto totalKb = getMemTotalKb())
                        info->memoryPercent = 100.0 * kb / *totalKb;
                } else if (line.compare(0,8,"Threads:")) {
                    std::istringstream iss(line);
                    std::string _;
                    iss >> _ >> info->numThreads;
                }
            }
        }

        /* 4. /proc/<pid>/io ---------------------------------------------------- */
        {
            std::ifstream f(fmt::format("/proc/{}/io", pid));
            std::string line;
            auto read_val = [](const std::string& l) -> std::size_t {
                std::istringstream iss(l);
                std::string _; std::size_t v;
                iss >> _ >> v;
                return v;
            };
            while (std::getline(f, line)) {
                if (line.compare(0,11,"read_bytes:"))
                    info->ioReadCount = read_val(line);
                else if (line.compare(0,12,"write_bytes:"))
                    info->ioWriteCount = read_val(line);
            }
        }

        /* 5. 网络 socket 计数 -------------------------------------------------- */
        {
            namespace fs = std::filesystem;
            fs::path fd_dir = fmt::format("/proc/{}/fd", pid);
            if (fs::exists(fd_dir)) {
                for (const auto& entry : fs::directory_iterator(fd_dir)) {
                    if (entry.is_symlink()) {
                        std::string target = fs::read_symlink(entry.path()).string();
                        if (target.compare(0,8,"socket:["))
                            ++info->netConnCount;
                    }
                }
            }
        }

        return info;
    } catch (...) {
        return nullptr;
    }
}

std::vector<std::unique_ptr<proc_info>> proc_collector(Job& job) {
    std::vector<std::unique_ptr<proc_info>> infos;
    for (int pid : job.JobPIDs) {
        if (pid <= 0) continue; // 无效 PID
        auto info = snapshotOf(job.JobID);
        if (!info) return infos; // 无法获取快照
        job.JobInfo[fmt::format("proc_info", pid)] = info.get(); // 存储到 JobInfo 中
        infos.push_back(std::move(info));
    }
    return infos;
}
