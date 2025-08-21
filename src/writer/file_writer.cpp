#include "writer/file_writer.hpp"

FileWriter::FileWriter(const std::string& path)
    : base_writer(8192), ofs_(path) {}

void FileWriter::flush_impl(const std::vector<write_data>& batch) {
    for (const auto& j : batch) {
        (void)j;           // 原实现未使用 j，保留写法
        ofs_ << '\n';
    }
    ofs_.flush();          // 强制落盘
}