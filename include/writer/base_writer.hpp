#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

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
    explicit base_writer(std::size_t buf_cap = 4096);
    virtual ~base_writer();

    void on_finish(std::string collect_name,
                   const Job& job,
                   const std::any data,
                   std::chrono::system_clock::time_point ts);

    OnFinish get_onFinishCallback();

protected:
    virtual void flush_impl(const std::vector<write_data>& batch);

private:
    struct Buffer;

    void write(const write_data& t);
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

    std::string name_ = "base";
    std::string type_;
};