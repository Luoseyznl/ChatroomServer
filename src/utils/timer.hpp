#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>
#include <functional>
#include <queue>
#include <mutex>
#include <memory>
#include <thread>
#include <condition_variable>

namespace utils {

class Timer {
public:
    struct Task {
        std::chrono::steady_clock::time_point execution_time;
        std::function<void()> callback;
        bool is_periodic;
        std::chrono::milliseconds period;

        Task(std::chrono::steady_clock::time_point time, 
             std::function<void()> cb,
             bool periodic = false,
             std::chrono::milliseconds p = std::chrono::milliseconds(0))
            : execution_time(time)
            , callback(cb)
            , is_periodic(periodic)
            , period(p) {}

        bool operator>(const Task& other) const {
            return execution_time > other.execution_time;
        }
    };

    explicit Timer();
    ~Timer();

    // 添加一次性定时任务
    void addOnceTask(std::chrono::milliseconds delay, std::function<void()> callback);

    // 添加周期性定时任务
    void addPeriodicTask(std::chrono::milliseconds delay, 
                        std::chrono::milliseconds period, 
                        std::function<void()> callback);

    // 启动定时器
    void start();

    // 停止定时器
    void stop();

private:
    void processTimerTasks();
    void scheduleNextTask();

    std::queue<Task> once_tasks_;     // 一次性任务队列
    std::queue<Task> periodic_tasks_;  // 周期性任务队列
    std::mutex mutex_;
    bool running_;
    std::condition_variable cv_;
    std::thread timer_thread_;
};

} // namespace utils

#endif // TIMER_HPP