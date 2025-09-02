// collector_registry.cpp
#include "collector/collector_registry.hpp"
#include "collector/collector_type.h"
#include <mutex>

CollectorRegistry& CollectorRegistry::instance() {
    static CollectorRegistry reg;
    return reg;
}

template <typename T>
void CollectorRegistry::registerCollector(std::string name) {
    factories_.emplace(std::move(name), []() -> std::unique_ptr<ICollector> {
        return std::make_unique<T>();
    });
}

CollectorHandle CollectorRegistry::createCollector(const std::string& name) const {
    auto it = factories_.find(name);
    if (it == factories_.end())
        return {};                     // 空句柄，调用方可判空

    auto impl = it->second();          // 创建采集器实例
    return {
        [impl = std::move(impl)](const nlohmann::json& cfg) { return impl->init(cfg); },
        [impl = std::move(impl)](const Job& job) { return impl->collect(job); },
        [impl = std::move(impl)](){ impl->deinit(); }
    };
}

std::vector<std::string> CollectorRegistry::list() const {
    std::vector<std::string> out;
    for (const auto& [k, _] : factories_) out.push_back(k);
    return out;
}

