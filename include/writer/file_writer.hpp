#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LOGGER_TRACE

#include <fstream>
#include <string>
#include <vector>
#include "writer/base_writer.hpp"

class FileWriter : public base_writer {
public:
    explicit FileWriter(std::string name, std::string type, std::string config_name);

protected:
    void flush_impl(const std::vector<write_data>& batch) override;

private:
    std::ofstream ofs_;
};