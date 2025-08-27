#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LOGGER_TRACE


#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <spdlog/spdlog.h>

#include "collector/collector_type.h"
#include "common/config.hpp"
#include "utils/nlohmann/json.hpp"

// 前向声明 fmt 为头文件减负；cpp 里再真正 include <fmt/core.h>
namespace fmt {}  // 占位，无实质依赖

using write_data = std::tuple<std::string,
                              const Job&,
                              const std::any,
                              std::chrono::system_clock::time_point>;

class base_writer
{
public:
    explicit base_writer(std::string name, std::string type, std::string config_name);
    virtual ~base_writer();

    void shutdown() {
        {
            std::lock_guard lg(mtx_);
            stop_ = true;
            need_flush_ = false;
        }
        
        cv_.notify_one();
        spdlog::info("base_writer: shutting down...");
        if (flush_thread_.joinable()){
            spdlog::info("base_writer: waiting for flush worker to finish...");
            flush_thread_.join();
        }
        flush_buffer(*front_);
        spdlog::info("base_writer: shutdown complete for writer '{}'", name_);
    }

    void on_finish(std::string collect_name,
                   const Job& job,
                   const std::any data,
                   std::chrono::system_clock::time_point ts);

    OnFinish get_onFinishCallback();

protected:
    virtual void flush_impl(const std::vector<write_data>& batch);
    void write(const write_data& t);
    std::string name_;
    std::string type_;
    std::string config_name_;

private:
    struct Buffer;

    
    void flush_worker();
    void flush_buffer(Buffer& buf);
    void trigger_async_flush();
    
    const std::size_t buf_capacity_;
    std::unique_ptr<Buffer> front_;
    std::unique_ptr<Buffer> back_;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::thread flush_thread_;
    bool stop_ = false;
    bool need_flush_ = false;

};