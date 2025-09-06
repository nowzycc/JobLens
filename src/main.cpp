#define SPDLOG_ACTIVE_LEVEL SPDLOG_LOGGER_TRACE
#include <iostream>
#include <thread>
#include <spdlog/spdlog.h>

#include "utils/cxxopts.hpp"

#include "common/distributed_node.hpp"
#include "common/config.hpp"
#include "common/job_starter.hpp"
#include "common/permission_opt.hpp"

#include "collector/job_registry.hpp"
#include "collector/job_info_collector.hpp"
#include <signal.h>

void onExitCallback(int pid, int exit_code) {
    JobInfoCollector::instance().shutdown();
    spdlog::info("Main: JobInfoCollector shutdown");
    DistributedNode::instance().stop();
    JobStarter::instance().shutdown();
    spdlog::info("Main: DistributedNode shutdown");
    spdlog::info("Main: Job with PID {} exited with code {}", pid, exit_code);
    exit(exit_code);
}

bool first_run = true;

void onBecomeMaster() {
    spdlog::info("Main: Node has become master.");
    if (first_run) {
        first_run = false;
        auto pid = JobStarter::instance().getChildPID();
        JobRegistry::instance().addJob({
            .JobID = 1,
            .JobPIDs = {pid},
            .JobCreateTime = std::chrono::system_clock::now()
        });

        // permission_opt::check_permission();

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

void onBecomeService(){
    spdlog::info("Main: app start as service mode.");
    JobInfoCollector::instance().start();
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
    print_logo();
    // 初始化日志系统
    const static auto log_level_map = std::map<std::string, spdlog::level::level_enum>{
        {"trace", spdlog::level::trace},
        {"debug", spdlog::level::debug},
        {"info", spdlog::level::info},
        {"warn", spdlog::level::warn},
        {"error", spdlog::level::err},
        {"critical", spdlog::level::critical},
        {"off", spdlog::level::off}
    };
    auto log_level = Config::instance().getString("lens_config", "log_level");
    auto it = log_level_map.find(log_level);
    if (it == log_level_map.end()) {
        log_level = "info"; // 默认 info 级别
    }
    auto level_enum = log_level_map.at(log_level);
    spdlog::set_level(level_enum); // 设置日志级别
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v"); // 设置日志格式
    
}

void get_main_mtx(){

}

bool already_running()
{
    auto PIDFILE = Config::instance().getString("lens_config", "lock_path");
    std::ifstream ifs(PIDFILE);
    if (ifs) {
        pid_t oldpid;
        ifs >> oldpid;
        if (oldpid > 0 && kill(oldpid, 0) == 0)   // 0 信号仅检测
            return true;                          // 同名进程存活
    }

    /* 把当前 pid 写进去 */
    std::ofstream ofs(PIDFILE);
    if (!ofs) {
        std::cerr << "cannot create " << PIDFILE << "\n";
        return false;                             // 保守起见，允许启动
    }
    ofs << getpid() << std::endl;
    return false;                                 // 可以继续跑
}

int main(int argc, char* argv[]) {
    cxxopts::Options options("JobLens", "A job monitor system");
    options.add_options()
        ("h,help", "Show help")
        ("c,config", "Configuration file path", cxxopts::value<std::string>()->default_value("config.yaml"))
        ("m,mode", "run mode (default: starter)", cxxopts::value<std::string>()->default_value("starter"))
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
    auto mode = result["mode"].as<std::string>();
    std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);
    Config::instance(result["config"].as<std::string>());

    init();

    if (mode.compare("starter") == 0){
        

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
    }

    if (mode.compare("service") == 0){
        if(already_running()){
            spdlog::critical("Main: Anothor JobLens has already started");
            return 0;
        }
        onBecomeService();
    }
    
    std::mutex mtx;
    std::condition_variable cv;
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return false; }); 
    return 0; // 永远不会到达这里
}