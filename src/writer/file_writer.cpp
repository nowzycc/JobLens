#include "writer/file_writer.hpp"

FileWriter::FileWriter(std::string name, std::string type, std::string config_name)
    : base_writer(name,type,config_name) {
        auto file_path = Config::instance().getString(config_name, "path");
        ofs_.open(file_path, std::ios::app);
    }

void FileWriter::flush_impl(const std::vector<write_data>& batch) {
    for (const auto& j : batch) {
        (void)j;           // 原实现未使用 j，保留写法
        ofs_ << '\n';
    }
    ofs_.flush();          // 强制落盘
}