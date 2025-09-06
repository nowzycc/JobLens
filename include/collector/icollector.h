// icollector.h
#pragma once
#include <string>
#include "collector_type.h"
#include "nlohmann/json.hpp"

class ICollector {
public:
    virtual ~ICollector() = default;

    // 生命周期
    virtual bool init(const nlohmann::json& config) = 0;   // 返回 false 表示失败
    virtual CollectResult collect(const Job& job)       = 0;
    virtual void deinit() noexcept                      = 0;
};

