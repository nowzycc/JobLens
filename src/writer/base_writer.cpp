#include "writer/base_writer.hpp"
#include <fmt/core.h>   // 仅在实现文件真正用到 fmt

// 内部类型完整定义
struct base_writer::Buffer
{
    explicit Buffer(std::size_t reserve) { vec.reserve(reserve); }
    void push_back(const write_data& j) { vec.push_back(j); }
    void clear() { vec.clear(); }
    std::size_t size() const { return vec.size(); }
    std::vector<write_data> vec;
};

// -------------------- 构造 / 析构 --------------------
base_writer::base_writer(std::size_t buf_cap)
    : buf_capacity_(buf_cap),
      front_(std::make_unique<Buffer>(buf_cap)),
      back_(std::make_unique<Buffer>(buf_cap)),
      flush_thread_(&base_writer::flush_worker, this)
{
}

base_writer::~base_writer()
{
    {
        std::lock_guard lg(mtx_);
        stop_ = true;
    }
    cv_.notify_one();
    if (flush_thread_.joinable())
        flush_thread_.join();

    flush_buffer(*front_);
}

// -------------------- 公有接口 --------------------
void base_writer::on_finish(std::string collect_name,
                            const Job& job,
                            const std::any data,
                            std::chrono::system_clock::time_point ts)
{
    auto t = std::make_tuple(std::move(collect_name), job, data, ts);
    write(t);
    trigger_async_flush();
}

OnFinish base_writer::get_onFinishCallback()
{
    return [this](const std::string& collect_name,
                  const Job& job,
                  const std::any data,
                  std::chrono::system_clock::time_point ts)
    { on_finish(collect_name, job, data, ts); };
}

// -------------------- 保护 / 私有实现 --------------------
void base_writer::flush_impl(const std::vector<write_data>&)
{
    // 默认空实现，留给派生类覆写
}

void base_writer::write(const write_data& t)
{
    front_->push_back(t);
    if (front_->size() >= buf_capacity_)
        trigger_async_flush();
}

void base_writer::flush_worker()
{
    std::unique_lock<std::mutex> lk(mtx_);
    for (;;)
    {
        cv_.wait(lk, [this] { return stop_ || need_flush_; });
        if (stop_)
            break;

        front_.swap(back_);
        need_flush_ = false;
        lk.unlock();

        flush_buffer(*back_);
        back_->clear();
        lk.lock();
    }
}

void base_writer::flush_buffer(Buffer& buf)
{
    if (!buf.vec.empty())
        flush_impl(buf.vec);
}

void base_writer::trigger_async_flush()
{
    {
        std::lock_guard lg(mtx_);
        need_flush_ = true;
    }
    cv_.notify_one();
}