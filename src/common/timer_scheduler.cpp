#include "common/timer_scheduler.hpp"
#include <iostream>
#include <spdlog/spdlog.h>

TimerScheduler::TimerScheduler(size_t numWorkers)
    : stop(false), nextId(0) {
    for (size_t i = 0; i < numWorkers; ++i) {
        workers.emplace_back([this] { workerLoop(); });
    }
    schedulerThread = std::thread([this] { schedulerLoop(); });
}

TimerScheduler::~TimerScheduler() {
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

void TimerScheduler::shutdown() {
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

size_t TimerScheduler::registerTimer(Duration delay, Task task) {
    return addTask(std::move(task), delay, false);
}

size_t TimerScheduler::registerRepeatingTimer(Duration interval, Task task) {
    return addTask(std::move(task), interval, true);
}

bool TimerScheduler::cancelTimer(size_t id) {
    std::lock_guard<std::mutex> lock(mtx);
    return taskMap.erase(id) > 0;
}

size_t TimerScheduler::addTask(Task task, Duration interval, bool repeat) {
    std::lock_guard<std::mutex> lock(mtx);
    auto id = nextId++;
    auto now = Clock::now();
    TimerTask timerTask{now + interval, interval, std::move(task), repeat, id};

    taskMap[id] = timerTask;
    tasks.push(timerTask);
    cv.notify_one();
    return id;
}

void TimerScheduler::schedulerLoop() {
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

void TimerScheduler::workerLoop() {
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
                spdlog::error("TimerScheduler: Task execution failed: {}",e.what());
                // std::cerr << "Task execution failed: " << e.what() << std::endl;
            }
        }
    }
}