#include "writer/writer_manager.hpp"

#include "common/config.hpp"
#include "utils/nlohmann/json.hpp"
#include "collector/collector_type.h"

#include <stdexcept>
#include <fmt/core.h>

writer_manager::writer_manager() {
    auto global_config = Config::instance();
    auto writer_names = global_config.getArray<std::string>("writer", "writers");
    for (const auto& name : writer_names) {
        auto writer_type = global_config.getString(name, "type");
        if (writer_type == "FileWriter") {
            auto path = global_config.getString(name, "path");
            auto writer = std::make_unique<FileWriter>(path);
            addWriter(std::move(writer));
        } else {
            throw std::runtime_error(fmt::format("Unknown writer type: {}", writer_type));
        }
    }
}

writer_manager::~writer_manager() {
    std::lock_guard lg(m_);
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