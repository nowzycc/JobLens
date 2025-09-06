#include "common/config.hpp"
#include <stdexcept>

Config::Config(const std::string& filePath)
{
    try {
        root_ = YAML::LoadFile(filePath);
        spdlog::info("Config: loaded configuration from {}", filePath);
    } catch (const YAML::BadFile& e) {
        spdlog::error("Config: failed to load configuration file {}: {}", filePath, e.what());
        throw std::runtime_error("Config: cannot open file: " + filePath);
    }
}

int Config::getInt(const std::string& parentKey,
                   const std::string& key) const
{
    try {
        return root_[parentKey][key].as<int>();
    } catch (const YAML::Exception& e) {
        spdlog::error("Config: error decoding int [{}][{}]: {}", parentKey, key, e.what());
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
        spdlog::error("Config: error decoding double [{}][{}]: {}", parentKey, key, e.what());
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
        spdlog::error("Config: error decoding bool [{}][{}]: {}", parentKey, key, e.what());
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
        spdlog::error("Config: error decoding string [{}][{}]: {}", parentKey, key, e.what());
        throw std::runtime_error("Config: missing or bad type for [" +
                                 parentKey + "][" + key + "]");
    }
}

YAML::Node Config::getRawNode(const std::string& parentKey) const
{
    try {
        return root_[parentKey];
    } catch (const YAML::Exception& e) {
        spdlog::error("Config: error decoding raw node [{}][{}]: {}", parentKey, e.what());
        throw std::runtime_error("Config: missing or bad type for [" +
                                 parentKey + "]");
    }
}

// 常见模板实例化
template std::vector<int>    Config::getArray<int>    (const std::string&, const std::string&) const;
template std::vector<double> Config::getArray<double> (const std::string&, const std::string&) const;
template std::vector<std::string> Config::getArray<std::string>(const std::string&, const std::string&) const;