#pragma once
#include <yaml-cpp/yaml.h>
#include <string>
#include <vector>

class Config {
public:
    // 从文件加载
    explicit Config(const std::string& filePath);

    // 基本类型读取
    int         getInt   (const std::string& parentKey,
                          const std::string& key) const;
    double      getDouble(const std::string& parentKey,
                          const std::string& key) const;
    bool        getBool  (const std::string& parentKey,
                          const std::string& key) const;
    std::string getString(const std::string& parentKey,
                          const std::string& key) const;

    // 数组读取
    template<typename T>
    std::vector<T> getArray(const std::string& parentKey,
                            const std::string& key) const;

    static Config& instance(std::string path = "") {
        static Config c = Config(initOnce(path));
        return c;
    }

private:
    static const std::string& initOnce(const std::string& path) {
        static std::string stored;
        if (!stored.empty()) return stored;          // 已初始化过
        if (path.empty())
            throw std::runtime_error("Config path not provided on first call");
        stored = path;
        return stored;
    }
    YAML::Node root_;
};