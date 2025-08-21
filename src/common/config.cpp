#include "common/config.hpp"
#include <stdexcept>

Config::Config(const std::string& filePath)
{
    try {
        root_ = YAML::LoadFile(filePath);
    } catch (const YAML::BadFile& e) {
        throw std::runtime_error("Config: cannot open file: " + filePath);
    }
}

int Config::getInt(const std::string& parentKey,
                   const std::string& key) const
{
    try {
        return root_[parentKey][key].as<int>();
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Config: missing or bad type for [" +
                                 parentKey + "][" + key + "]");
    }
}

double Config::getDouble(const std::string& parentKey,
                         const std::string& key) const
{
    try {
        return root_[parentKey][key].as<double>();
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Config: missing or bad type for [" +
                                 parentKey + "][" + key + "]");
    }
}

bool Config::getBool(const std::string& parentKey,
                     const std::string& key) const
{
    try {
        return root_[parentKey][key].as<bool>();
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Config: missing or bad type for [" +
                                 parentKey + "][" + key + "]");
    }
}

std::string Config::getString(const std::string& parentKey,
                              const std::string& key) const
{
    try {
        return root_[parentKey][key].as<std::string>();
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Config: missing or bad type for [" +
                                 parentKey + "][" + key + "]");
    }
}

template<typename T>
std::vector<T> Config::getArray(const std::string& parentKey,
                                const std::string& key) const
{
    try {
        return root_[parentKey][key].as<std::vector<T>>();
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Config: missing or bad type for [" +
                                 parentKey + "][" + key + "]");
    }
}

// 常见模板实例化
template std::vector<int>    Config::getArray<int>    (const std::string&, const std::string&) const;
template std::vector<double> Config::getArray<double> (const std::string&, const std::string&) const;
template std::vector<std::string> Config::getArray<std::string>(const std::string&, const std::string&) const;