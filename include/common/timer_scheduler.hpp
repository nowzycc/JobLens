#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <atomic>
#include <future>
#include <unordered_map>

class TimerScheduler {
public:
    using Task = std::function<void()>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::milliseconds;

    struct TimerTask {
        TimePoint nextRun;
        Duration interval;
        Task task;
        bool repeat;
        size_t id;

        bool operator<(const TimerTask& other) const {
            return nextRun > other.nextRun;
        }
    };

    explicit TimerScheduler(size_t numWorkers = 1)
        : stop(false), nextId(0) {
        for (size_t i = 0; i < numWorkers; ++i) {
            workers.emplace_back([this] { workerLoop(); });
        }
        schedulerThread = std::thread([this] { schedulerLoop(); });
    }

    ~TimerScheduler() {
        stop = true;
        cv.notify_all();
        schedulerCv.notify_all();
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        if (schedulerThread.joinable()) {
            schedulerThread.join();
        }
    }

    // 注册单次定时任务
    size_t registerTimer(Duration delay, Task task) {
        return addTask(std::move(task), delay, false);
    }

    // 注册重复定时任务
    size_t registerRepeatingTimer(Duration interval, Task task) {
        return addTask(std::move(task), interval, true);
    }

    // 取消任务
    bool cancelTimer(size_t id) {
        std::lock_guard<std::mutex> lock(mtx);
        return taskMap.erase(id) > 0;
    }

private:
    size_t addTask(Task task, Duration interval, bool repeat) {
        std::lock_guard<std::mutex> lock(mtx);
        auto id = nextId++;
        auto now = Clock::now();
        TimerTask timerTask{now + interval, interval, std::move(task), repeat, id};
        
        taskMap[id] = timerTask;
        tasks.push(timerTask);
        cv.notify_one();
        return id;
    }

    void schedulerLoop() {
        while (!stop) {
            std::unique_lock<std::mutex> lock(mtx);
            if (tasks.empty()) {
                cv.wait(lock, [this] { return stop || !tasks.empty(); });
                if (stop) break;
            }

            auto now = Clock::now();
            if (!tasks.empty() && tasks.top().nextRun <= now) {
                auto task = tasks.top();
                tasks.pop();
                lock.unlock();

                taskQueue.push(task.task);
                schedulerCv.notify_one();

                if (task.repeat) {
                    task.nextRun = now + task.interval;
                    lock.lock();
                    if (taskMap.count(task.id)) {
                        tasks.push(task);
                    }
                    lock.unlock();
                } else {
                    lock.lock();
                    taskMap.erase(task.id);
                }
            } else {
                cv.wait_until(lock, tasks.top().nextRun);
            }
        }
    }

    void workerLoop() {
        while (!stop) {
            std::unique_lock<std::mutex> lock(queueMtx);
            schedulerCv.wait(lock, [this] { return stop || !taskQueue.empty(); });
            if (stop) break;

            if (!taskQueue.empty()) {
                auto task = std::move(taskQueue.front());
                taskQueue.pop();
                lock.unlock();
                
                try {
                    task();
                } catch (const std::exception& e) {
                    std::cerr << "Task execution failed: " << e.what() << std::endl;
                }
            }
        }
    }

    std::vector<std::thread> workers;
    std::thread schedulerThread;
    
    std::priority_queue<TimerTask> tasks;
    std::unordered_map<size_t, TimerTask> taskMap;
    
    std::queue<Task> taskQueue;
    
    std::mutex mtx;
    std::mutex queueMtx;
    std::condition_variable cv;
    std::condition_variable schedulerCv;
    
    std::atomic<bool> stop;
    std::atomic<size_t> nextId;
};