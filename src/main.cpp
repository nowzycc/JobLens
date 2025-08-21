#include <iostream>

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
    std::cout << "Node has become master." << std::endl;
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

void onBecomeSlave() {
    std::cout << "Node has become slave." << std::endl;
    if (first_run) {
        first_run = false;

    }
    else{
        // 从系统设计角度来说不会运行到这里
    }
}

int main(int argc, char* argv[]) {
    print_logo();
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

    DistributedNode::instance().start();

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

    return 0;
}