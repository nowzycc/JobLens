#define SPDLOG_ACTIVE_LEVEL SPDLOG_LOGGER_TRACE
#include <iostream>
#include <spdlog/spdlog.h>

#include "utils/cxxopts.hpp"

#include "common/distributed_node.hpp"
#include "common/config.hpp"
#include "common/job_starter.hpp"
#include "common/permission_opt.hpp"

#include "collector/job_info_collector.hpp"

void onExitCallback(int pid, int exit_code) {
    JobInfoCollector::instance().shutdown();
    DistributedNode::instance().stop();
    std::cout << "Job with PID " << pid << " exited with code " << exit_code << std::endl;
    exit(exit_code);
}

bool first_run = true;

void onBecomeMaster() {
    spdlog::info("Main: Node has become master.");
    if (first_run) {
        first_run = false;
        auto pid = JobStarter::instance().getChildPID();
        JobInfoCollector::instance().addJob({
            .JobID = 1, 
            .JobPIDs = {pid},
            .JobCreateTime = std::chrono::system_clock::now()
        });

        permission_opt::check_permission();

        JobInfoCollector::instance().start();
    }
    else{
        // JobInfoCollector::instance().restore_from_snapshot();
    }
    
}

void onBecomeSlave() {
    std::cout << "Node has become slave." << std::endl;
    if (first_run) {
        first_run = false;

    }
    else{
        // 从系统设计角度来说不会运行到这里
    }
}

void print_logo(){
    std::cout<< R"(    _____          __       __                                 
   |     \        |  \     |  \
    \$$$$$ ______ | $$____ | $$      ______  _______   _______ 
      | $$/      \| $$    \| $$     /      \|       \ /       \
 __   | $|  $$$$$$| $$$$$$$| $$    |  $$$$$$| $$$$$$$|  $$$$$$$
|  \  | $| $$  | $| $$  | $| $$    | $$    $| $$  | $$\$$    \
| $$__| $| $$__/ $| $$__/ $| $$____| $$$$$$$| $$  | $$_\$$$$$$\
 \$$    $$\$$    $| $$    $| $$     \$$     | $$  | $|       $$
  \$$$$$$  \$$$$$$ \$$$$$$$ \$$$$$$$$\$$$$$$$\$$   \$$\$$$$$$$ 
                                                               
                                                               
                                                               )" << std::endl;
}

void init() {
    // 初始化日志系统
    spdlog::set_level(spdlog::level::info); // 设置日志级别
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v"); // 设置日志格式
    print_logo();
}

int main(int argc, char* argv[]) {
    init();
    cxxopts::Options options("JobLens", "A job monitor system");
    options.add_options()
        ("h,help", "Show help")
        ("c,config", "Configuration file path", cxxopts::value<std::string>()->default_value("config.yaml"))
        ("m,monitor", "Monitor mode (default: true)", cxxopts::value<bool>()->default_value("true"))
        ("e,exec", "Executable to run", cxxopts::value<std::string>())
        ("a,args", "Arguments for the executable", cxxopts::value<std::vector<std::string>>()->default_value(""));
    
    auto result = options.parse(argc, argv);
    if (argc < 2) {
        std::cout << options.help() << std::endl;
        return 0;
    }
    if (result.count("help")) {
        std::cout << options.help() << std::endl;
        return 0;
    }
    
    Config::instance(result["config"].as<std::string>());
    
    DistributedNode::instance().set_become_master_callback(onBecomeMaster);

    DistributedNode::instance().set_become_slave_callback(onBecomeSlave);

    JobStarter::instance().setCallback(onExitCallback);

    auto ret = JobStarter::instance().launch({
        .exe = result["exec"].as<std::string>(),
        .args = result["args"].as<std::vector<std::string>>(),
        .timeout = std::chrono::milliseconds(5000) // 可选超时
    });
    
    if (!ret) {
        std::cerr << "Failed to launch job." << std::endl;
        return 1;
    }

    DistributedNode::instance().start();

    return 0;
}