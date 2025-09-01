// collector_registry.h
#pragma once
#include <unordered_map>
#include <functional>
#include <memory>
#include "icollector.h"

class CollectorRegistry {
public:
    // 单例（可选）；也可 main() 手动构造
    static CollectorRegistry& instance();

    // 注册模板：把任意类型 T 登记到 name 名下
    template <typename T>
    void registerCollector(std::string name);

    // 工厂：根据名称 + JSON 配置生成一个“已初始化”的可调用对象
    // 返回 nullptr 表示失败
    std::unique_ptr<CollectFunc> createCollector(
        const std::string& name,
        const nlohmann::json& config) const;

    // 列举已注册采集器（调试用）
    std::vector<std::string> list() const;

private:
    using Factory = std::function<std::unique_ptr<ICollector>()>;

    CollectorRegistry() = default;
    std::unordered_map<std::string, Factory> factories_;
};

template <typename T>
struct AutoReg {
    AutoReg(const char* name) {
        CollectorRegistry::instance().registerCollector<T>(name);
    }
};