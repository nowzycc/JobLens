// elasticsearch_writer.h
#pragma once
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LOGGER_TRACE

#include "writer/base_writer.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>

class ESWriter : public base_writer
{
public:
    struct options
    {
        std::string host         = "localhost";
        int         port         = 9200;
        std::string index_prefix = "collector";
        std::size_t batch_size   = 500;   // 达到多少条就打包
    };

    explicit ESWriter(std::string name, std::string type, std::string config_name);

    ~ESWriter() override;

    /* 重写父类 write，用于“本地缓冲 -> 父类缓冲” */
    void write(const write_data& w);

    /* 真正写 ES：被 base_writer 后台线程调用 */
    void flush_impl(const std::vector<write_data>& batch) override;

private:
    bool post_bulk(const std::string& bulk_body);
    bool test_server();
    std::string try_get_index_name(const write_data& w);
    bool try_parse_data(const std::string& collector_name, const std::any& data, nlohmann::json& out);
    int write_timeout;
    options opt_;
    std::vector<write_data> local_buf_;   // 子类私有缓冲
    std::mutex local_mtx_;                // 保护 local_buf_
    CURL* curl_ = nullptr;
};