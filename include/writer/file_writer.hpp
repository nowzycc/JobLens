#pragma once
#include <fstream>
#include <string>
#include <vector>
#include "writer/base_writer.hpp"

class FileWriter : public base_writer {
public:
    explicit FileWriter(const std::string& path);

protected:
    void flush_impl(const std::vector<write_data>& batch) override;

private:
    std::ofstream ofs_;
};