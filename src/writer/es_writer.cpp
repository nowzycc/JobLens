// elasticsearch_writer.cpp
#include "writer/es_writer.hpp"
#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <date/date.h>
#include "common/config.hpp"
#include "collector/collector_utils.hpp"
#include "collector/proc_collector_func.hpp"

using json = nlohmann::json;

static size_t discard_cb(char*, size_t size, size_t n, void*) { return size * n; }

/* ---------- 构造/析构 ---------- */
ESWriter::ESWriter(std::string name, std::string type, std::string config_name)
    : base_writer(name,type,config_name)
{
    opt_.batch_size = Config::instance().getInt(config_name, "batch_size");
    opt_.host = Config::instance().getString(config_name, "host");
    opt_.port = Config::instance().getInt(config_name, "port");
    write_timeout = Config::instance().getInt(config_name, "write_timeout");
    try
    {
        opt_.index_prefix = Config::instance().getString(config_name, "index_prefix");
    }
    catch(const std::exception& e)
    {
        opt_.index_prefix = "collector";
        spdlog::warn("elasticsearch_writer: no index_prefix configured, using default 'collector'");
    }
    spdlog::debug("elasticsearch_writer: initializing curl...");
    curl_global_init(CURL_GLOBAL_ALL);
    spdlog::debug("elasticsearch_writer: curl_global_init done");
    curl_ = curl_easy_init();
    if (!curl_) throw std::runtime_error("curl_easy_init failed");
    if (!test_server()) {
        throw std::runtime_error("Failed to connect to Elasticsearch server at " +
                                 opt_.host + ":" + std::to_string(opt_.port));
    }
    local_buf_.reserve(opt_.batch_size);
    spdlog::info("elasticsearch_writer: initialized with host={} port={} index_prefix='{}' batch_size={}",
                 opt_.host, opt_.port, opt_.index_prefix, opt_.batch_size);
}

ESWriter::~ESWriter()
{
    if (curl_) curl_easy_cleanup(curl_);
    curl_global_cleanup();
}

bool ESWriter::test_server()
{
    if (!curl_) return false;

    const std::string url =
        fmt::format("http://{}:{}/", opt_.host, opt_.port);
    spdlog::debug("elasticsearch_writer: testing connection to {}", url);
    curl_easy_reset(curl_);
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_NOBODY, 1L); // HEAD 请求
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, discard_cb); // 丢弃响应体
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, write_timeout); // 设置超时

    CURLcode res = curl_easy_perform(curl_);
    
    if (res != CURLE_OK) {
        spdlog::error("elasticsearch_writer: curl_easy_perform() failed: {}", curl_easy_strerror(res));
        return false;
    }

    long response_code = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code != 200) {
        spdlog::error("elasticsearch_writer: server responded with code {}", response_code);
        return false;
    }

    spdlog::info("elasticsearch_writer: successfully connected to Elasticsearch at {}:{}", opt_.host, opt_.port);
    return true;
}

/* ---------- 子类缓冲 ---------- */
void ESWriter::write(const write_data& w)
{
    std::lock_guard lg(local_mtx_);
    spdlog::debug("elasticsearch_writer: write called for collector '{}'", std::get<0>(w));
    local_buf_.push_back(w);

    if (local_buf_.size() >= opt_.batch_size)
    {
        /* 把整批数据一次性交给父类；父类会立即触发后台 flush */
        std::vector<write_data> tmp = std::move(local_buf_);
        local_buf_.clear();
        local_buf_.reserve(opt_.batch_size);

        /* 逐条写入父类缓冲区（父类内部会再次合并）*/
        for (auto& item : tmp)
            this->base_writer::write(std::move(item));
    }
}

std::string ESWriter::try_get_index_name(const write_data& w)
{
    static std::string last_index_name = "default";
    static std::string last_collector_name = "";
    if (std::get<0>(w) == last_collector_name)
        return last_index_name;
    last_collector_name = std::get<0>(w);
    struct es_index_config
    {
        std::string collector_name;
        std::string index_name;
    };
    static const auto config_indexs = Config::instance().getArray<es_index_config>(
        config_name_, "indexs",
        [](const YAML::Node& node) {
            es_index_config c;
            c.collector_name = node["collector_name"].as<std::string>();
            c.index_name = node["index_name"].as<std::string>();
            return c;
        });
        
    for (const auto& c : config_indexs) {
        if (c.collector_name == last_collector_name) {
            last_index_name = c.index_name;
            return last_index_name;
        }
    }
    return "default";
}

bool ESWriter::try_parse_data(const std::string& collector_name, const std::any& data, json& out)
{
    auto type = collector_utils::get_type_from_name(collector_name);
    if (type.compare(COLLECTOR_TYPE_PROC) == 0) {
        try {
            if(data.has_value() == false) {
                spdlog::warn("elasticsearch_writer: empty data for collector '{}'", collector_name);
                return false;
            }
            auto parsed = std::any_cast<std::vector<std::shared_ptr<proc_collector::proc_info>>>(data);
            for (const auto& info : parsed) {
                json j;
                j["type"] = info->type;
                j["pid"] = info->pid;
                j["name"] = info->name;
                j["ppid"] = info->ppid;
                j["cpuPercent"] = info->cpuPercent;
                j["memoryRss"] = info->memoryRss;
                j["memoryPercent"] = info->memoryPercent;
                j["numThreads"] = info->numThreads;
                j["ioReadCount"] = info->ioReadCount;
                j["ioWriteCount"] = info->ioWriteCount;
                j["netConnCount"] = info->netConnCount;
                j["status"] = info->status;
                out.push_back(j);
            }
            return true;
        }
        catch (const std::bad_any_cast& e) {
            spdlog::error("elasticsearch_writer: bad_any_cast for collector '{}': {}", collector_name, e.what());
        }
    }
    return false;
}

/* ---------- 真正写 ES ---------- */
void ESWriter::flush_impl(const std::vector<write_data>& batch)
{
    if (batch.empty()) return;

    std::ostringstream body;
    for (const auto& [collect_name, job, any_data, ts] : batch)
    {
        json action;
        auto index_name = try_get_index_name(batch[0]);
        if (index_name != "default") {
            action["index"]["_index"] = index_name;
        } else {
            spdlog::warn("elasticsearch_writer: using default index for collector '{}'", opt_.index_prefix + "_" + collect_name);
            action["index"]["_index"] = opt_.index_prefix + "_" + collect_name;
        }
        body << action.dump() << '\n';

        json src;
        src["@timestamp"] = date::format("%FT%T%z", date::floor<std::chrono::seconds>(ts));
        src["collector_name"] = collect_name;
        src["hostname"] = collector_utils::get_hostname();
        json jobj;
        try_parse_data(collect_name, any_data, jobj);
        spdlog::debug("elasticsearch_writer: document to index: {}", jobj.dump());
        src["data"] = jobj;
        body << src.dump() << '\n';
    }

    if (!post_bulk(body.str()))
        spdlog::warn("elasticsearch_writer: flush failed, batch={}", batch.size());
    else
        spdlog::debug("elasticsearch_writer: flushed {}", batch.size());
}

/* ---------- libcurl POST ---------- */


bool ESWriter::post_bulk(const std::string& bulk)
{
    if (!curl_) return false;

    const std::string url =
        fmt::format("http://{}:{}/_bulk", opt_.host, opt_.port);

    curl_easy_reset(curl_);
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, bulk.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, bulk.size());
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, write_timeout); // 设置超时

    struct curl_slist* hdrs = nullptr;
    hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, hdrs);
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, discard_cb);

    CURLcode rc = curl_easy_perform(curl_);
    curl_slist_free_all(hdrs);

    if (rc != CURLE_OK)
    {
        spdlog::error("curl: {}", curl_easy_strerror(rc));
        return false;
    }

    long code = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &code);
    return code == 200 || code == 201;
}