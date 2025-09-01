// collector_registry.cpp
#include "collector/collector_registry.hpp"
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

std::unique_ptr<CollectFunc>
CollectorRegistry::createCollector(const std::string& name,
                                   const nlohmann::json& config) const {
    auto it = factories_.find(name);
    if (it == factories_.end()) return nullptr;

    auto collector = it->second();             // 构造
    if (!collector->init(config)) return nullptr;

    // 返回一个 lambda：内部持有 collector 的独占所有权
    return std::make_unique<CollectFunc>(
        [c = std::move(collector)](const Job& job) mutable -> CollectResult {
            return c->collect(job);
        });
}

std::vector<std::string> CollectorRegistry::list() const {
    std::vector<std::string> out;
    for (const auto& [k, _] : factories_) out.push_back(k);
    return out;
}

