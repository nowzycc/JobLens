#include "collector/proc_collector_func.hpp"
#include <fmt/core.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unistd.h>
#include <dirent.h>
#include <limits>
#include <vector>

#include "collector/collector_type.h"
#include <any>

namespace proc_collector {

/* ---------- 内部工具 ---------- */
static std::size_t pageSize() {
    static const std::size_t sz = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    return sz;
}

/* ---------- 公开接口 ---------- */
std::optional<std::size_t> getMemTotalKb() {
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

std::unique_ptr<proc_info> snapshotOf(int pid) {
    try {
        auto info = std::make_unique<proc_info>();
        info->pid = pid;

        /* 1. /proc/<pid>/stat ------------------------------------------------- */
        {
            std::ifstream f(fmt::format("/proc/{}/stat", pid));
            if (!f) return nullptr;

            std::string line;
            std::getline(f, line);
            std::istringstream iss(line);

            int _pid;
            iss >> _pid;
            if (iss.peek() == ' ') iss.ignore();   // 去掉空格
            if (iss.peek() == '(') {               // 进程名可能带空格
                iss.ignore();
                std::getline(iss, info->name, ')');
            } else {
                iss >> info->name;
            }

            char state;
            iss >> state >> info->ppid;            // 第 2 列 state，第 3 列 ppid

            /* 跳到第 14 列（utime） */
            unsigned long long utime, stime;
            for (int i = 0; i < 11; ++i)
                iss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
            iss >> utime >> stime;                 // 14,15

            unsigned long long starttime;
            for (int i = 0; i < 4; ++i)
                iss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
            iss >> starttime;                      // 22
        }

        /* 2. /proc/<pid>/statm ----------------------------------------------- */
        {
            std::ifstream f(fmt::format("/proc/{}/statm", pid));
            if (f) {
                unsigned long vmsize, rss;
                f >> vmsize >> rss;
                info->memoryRss = rss * pageSize();
            }
        }

        /* 3. /proc/<pid>/status ---------------------------------------------- */
        {
            std::ifstream f(fmt::format("/proc/{}/status", pid));
            if (!f) return nullptr;

            std::string line;
            while (std::getline(f, line)) {
                if (line.compare(0, 6, "VmRSS:") == 0) {
                    std::istringstream iss(line);
                    std::string _;
                    std::size_t kb;
                    iss >> _ >> kb;
                    if (auto totalKb = getMemTotalKb())
                        info->memoryPercent = 100.0 * kb / *totalKb;
                } else if (line.compare(0,8,"Threads:") == 0) {
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
            while (std::getline(f, line)) {
                if (line.compare(0,11,"read_bytes:") == 0) {
                    std::istringstream iss(line);
                    std::string _; std::size_t v;
                    iss >> _ >> v;
                    info->ioReadCount = static_cast<int>(v);
                } else if (line.compare(0,12,"write_bytes:") == 0) {
                    std::istringstream iss(line);
                    std::string _; std::size_t v;
                    iss >> _ >> v;
                    info->ioWriteCount = static_cast<int>(v);
                }
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
                        if (target.compare(0,8,"socket:[") == 0)
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

std::any collect(Job& job) {
    std::vector<std::shared_ptr<proc_info>> infos;
    for (int pid : job.JobPIDs) {
        if (pid <= 0) continue;
        auto info = snapshotOf(pid);
        if (!info) continue;
        job.JobInfo[fmt::format("proc_info_{}", pid)] = info.get();
        infos.emplace_back(std::move(info));
    }
    
    std::any a = std::move(infos); 

    return a;
}

} 