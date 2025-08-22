#include "writer/writer_manager.hpp"

#include "common/config.hpp"
#include "utils/nlohmann/json.hpp"
#include "collector/collector_type.h"

#include <stdexcept>
#include <fmt/core.h>

const char* WRITER_TYPE_FILE = "FileWriter";

writer_manager::writer_manager() {
    struct Writer{
        std::string name;
        std::string type;
        std::string config;
    };
    auto global_config = Config::instance();
    spdlog::info("writer_manager: initializing...");
    auto writers = global_config.getArray<Writer>("writers_config", "writers", 
        [](const YAML::Node& node) {
            Writer w;
            w.name = node["name"].as<std::string>();
            w.type = node["type"].as<std::string>();
            w.config = node["config"].as<std::string>();
            return w;
        });
    spdlog::info("writer_manager: found {} writers", writers.size());
    for (const auto& writer : writers) {
        auto writer_type = writer.type;
        auto name = writer.name;
        auto config = writer.config;
        if (writer_type == WRITER_TYPE_FILE) {
            auto path = global_config.getString(config, "path");
            auto writer_handle = std::make_unique<FileWriter>(path);
            addWriter(std::move(writer_handle));
        } else {
            throw std::runtime_error(fmt::format("Unknown writer type: {}", writer_type));
        }
    }
    spdlog::info("writer_manager: initialized with {} writers", writers_.size());
}

writer_manager::~writer_manager() {
    std::lock_guard lg(m_);
    writers_.clear();
}

void writer_manager::shutdown() {
    std::lock_guard lg(m_);
    spdlog::info("writer_manager: shutting down...");
    for (auto& writer : writers_) {
        writer->shutdown();
    }
    writers_.clear();
}

std::vector<OnFinish> writer_manager::get_onFinishCallbacks() {
    std::lock_guard lg(m_);
    std::vector<OnFinish> callbacks;
    for (const auto& writer : writers_) {
        callbacks.push_back(writer->get_onFinishCallback());
    }
    return callbacks;
}

void writer_manager::addWriter(std::unique_ptr<base_writer> writer) {
    std::lock_guard lg(m_);
    writers_.push_back(std::move(writer));
}

writer_manager& writer_manager::instance() {
    static writer_manager instance;
    return instance;
}