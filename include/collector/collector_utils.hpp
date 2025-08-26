#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LOGGER_TRACE

#include <string>
#include <unistd.h>
#include "collector/collector_type.h"
#include "common/config.hpp"

namespace collector_utils
{
    static std::string get_type_from_name(const std::string& collector_name)
    {
        struct Collector {
            std::string name;
            std::string type;
            std::string config;
        };
        static std::unordered_map<std::string, std::string> name_to_type;
        static bool initialized = false;
        if (!initialized) {
            auto global_config = Config::instance();
            auto collectors = global_config.getArray<Collector>("collectors_config", "collectors",
            [](const YAML::Node& node) {
                Collector c;
                c.name = node["name"].as<std::string>();
                c.type = node["type"].as<std::string>();
                c.config = node["config"].as<std::string>();
                return c;
            });
            for (const auto& collectors : collectors) {
                name_to_type[collectors.name] = collectors.type;
            }
            initialized = true;
        }
        auto it = name_to_type.find(collector_name);
        if (it != name_to_type.end()) {
            return it->second;
        }
        return "unknown";
    }

    static std::string get_hostname()
    {
        static std::string hostname;
        if (hostname.empty()) {
            char buf[256];
            if (gethostname(buf, sizeof(buf)) == 0) {
                hostname = buf;
            } else {
                hostname = "unknown";
            }
        }
        return hostname;
    }
} // namespace collector_utils
