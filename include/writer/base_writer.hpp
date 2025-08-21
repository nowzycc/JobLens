#pragma once
#include <string>
#include <iostream>
#include <chrono>
#include <functional>
#include <fmt/core.h>

#include "utils/nlohmann/json.hpp"
#include "common/config.hpp"

#include "collector/collector_type.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

using write_data = std::tuple<std::string,const Job&,const void*, std::chrono::system_clock::time_point>;

class base_writer
{
public:
    explicit base_writer(std::size_t buf_cap = 4096)
        : buf_capacity_(buf_cap),
          front_(new Buffer(buf_cap)),
          back_(new Buffer(buf_cap)),
          flush_thread_(&base_writer::flush_worker, this) {}

    virtual ~base_writer()
    {
        {
            std::lock_guard lg(mtx_);
            stop_ = true;
        }
        cv_.notify_one();
        if (flush_thread_.joinable())
            flush_thread_.join();

        flush_buffer(*front_); // 析构时把剩余数据刷掉
    }

    // 每次 Job 完成都会调用，确保立即刷新
    void on_finish(
        std::string collect_name,
        const Job &job,
        const void* data,
        std::chrono::system_clock::time_point ts)
    {
        auto t = std::make_tuple(collect_name, job, data, ts);
        write(t);              // 先把数据写进缓冲
        trigger_async_flush(); // 立即唤醒后台线程
    }

    auto get_onFinishCallback()
    {
        return [this](
            const std::string collect_name,
            const Job &job,
            const void* data,
            std::chrono::system_clock::time_point ts)
        { on_finish(collect_name, job, data, ts); };
    }

protected:
    // 派生类实现真正的写出逻辑（磁盘 / 网络 / 控制台）
    virtual void flush_impl(const std::vector<write_data> &batch)
    {
    }

private:
    struct Buffer
    {
        explicit Buffer(std::size_t reserve) { vec.reserve(reserve); }
        void push_back(const write_data &j) { vec.push_back(j); }
        void clear() { vec.clear(); }
        std::size_t size() const { return vec.size(); }
        std::vector<write_data> vec;
    };

    // 派生类写数据时直接调用
    void write(const write_data& t)
    {
        front_->push_back(t);
        // 如果前端缓冲快满了，提前触发一次异步 flush
        if (front_->size() >= buf_capacity_)
        {
            trigger_async_flush();
        }
    }

    void flush_worker()
    {
        std::unique_lock<std::mutex> lk(mtx_);
        for (;;)
        {
            cv_.wait(lk, [this]
                     { return stop_ || need_flush_; });
            if (stop_)
                break;

            // 交换前后台缓冲
            front_.swap(back_);
            need_flush_ = false;
            lk.unlock();

            flush_buffer(*back_); // 真正耗时操作在锁外执行
            back_->clear();
            lk.lock();
        }
    }

    void flush_buffer(Buffer &buf)
    {
        if (!buf.vec.empty())
            flush_impl(buf.vec);
    }

    void trigger_async_flush()
    {
        {
            std::lock_guard lg(mtx_);
            need_flush_ = true;
        }
        cv_.notify_one();
    }

    // 数据成员
    const std::size_t buf_capacity_;
    std::unique_ptr<Buffer> front_;
    std::unique_ptr<Buffer> back_;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::thread flush_thread_;
    bool stop_ = false;
    bool need_flush_ = false;

    // 其它原字段
    std::string name_ = "base";
    std::string type_;
};