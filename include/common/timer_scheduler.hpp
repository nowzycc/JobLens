#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

class TimerScheduler {
public:
    using Task      = std::function<void()>;
    using Clock     = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration  = std::chrono::milliseconds;

    struct TimerTask {
        TimePoint nextRun;
        Duration  interval;
        Task      task;
        bool      repeat;
        size_t    id;

        bool operator<(const TimerTask& other) const {
            return nextRun > other.nextRun;
        }
    };

    explicit TimerScheduler(size_t numWorkers = 1);
    ~TimerScheduler();

    void shutdown();

    // 注册单次定时任务
    size_t registerTimer(Duration delay, Task task);

    // 注册重复定时任务
    size_t registerRepeatingTimer(Duration interval, Task task);

    // 取消任务
    bool cancelTimer(size_t id);

private:
    size_t addTask(Task task, Duration interval, bool repeat);

    void schedulerLoop();
    void workerLoop();

    std::vector<std::thread> workers;
    std::thread              schedulerThread;

    std::priority_queue<TimerTask>         tasks;
    std::unordered_map<size_t, TimerTask>  taskMap;

    std::queue<Task> taskQueue;

    std::mutex              mtx;
    std::mutex              queueMtx;
    std::condition_variable cv;
    std::condition_variable schedulerCv;

    std::atomic<bool>  stop;
    std::atomic<size_t> nextId;
};
