#pragma once
#include <fstream>
#include <string>
#include "writer/base_writer.hpp"


class FileWriter : public base_writer {
public:
    explicit FileWriter(const std::string& path)
        : base_writer(8192), ofs_(path) {}
protected:
    void flush_impl(const std::vector<write_data>& batch) {
        for (auto& j : batch) ofs_ << '\n';
        ofs_.flush();   // 强制落盘
    }
private:
    std::ofstream ofs_;
};