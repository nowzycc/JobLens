#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <functional>

#include "writer/file_writer.hpp"
#include "writer/es_writer.hpp"

// 前置声明，避免不必要的头文件依赖
class base_writer;

class writer_manager {
private:
    std::vector<std::unique_ptr<base_writer>> writers_;
    std::mutex m_;

    void addWriter(std::unique_ptr<base_writer> writer);

public:
    writer_manager();
    ~writer_manager();
    
    void shutdown();

    std::vector<OnFinish> get_onFinishCallbacks();

    static writer_manager& instance();
};