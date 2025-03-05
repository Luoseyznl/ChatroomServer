#include "timer.hpp"

namespace utils {

Timer::Timer()
    : running_(false) {}

Timer::~Timer() {
    stop();
}

void Timer::addOnceTask(std::chrono::milliseconds delay, std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto execution_time = std::chrono::steady_clock::now() + delay;
    Task task(execution_time, std::move(callback));
    once_tasks_.push(std::move(task));
    cv_.notify_one();
}

void Timer::addPeriodicTask(std::chrono::milliseconds delay,
                           std::chrono::milliseconds period,
                           std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto execution_time = std::chrono::steady_clock::now() + delay;
    Task task(execution_time, std::move(callback), true, period);
    periodic_tasks_.push(std::move(task));
    cv_.notify_one();
}

void Timer::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        running_ = true;
        timer_thread_ = std::thread([this] { processTimerTasks(); });
    }
}

void Timer::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        cv_.notify_one();
    }
    if (timer_thread_.joinable()) {
        timer_thread_.join();
    }
}

void Timer::processTimerTasks() {
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        
        if (!running_) {
            break;
        }

        if (once_tasks_.empty() && periodic_tasks_.empty()) {
            cv_.wait(lock, [this] { return !running_ || !once_tasks_.empty() || !periodic_tasks_.empty(); });
            continue;
        }

        auto now = std::chrono::steady_clock::now();
        Task* earliest_task = nullptr;
        auto earliest_time = std::chrono::steady_clock::time_point::max();
        bool is_from_periodic = false;

        // 检查一次性任务队列
        if (!once_tasks_.empty()) {
            Task& task = once_tasks_.front();
            if (task.execution_time < earliest_time) {
                earliest_time = task.execution_time;
                earliest_task = new Task(std::move(task));
                is_from_periodic = false;
            }
        }

        // 检查周期性任务队列
        if (!periodic_tasks_.empty()) {
            Task& task = periodic_tasks_.front();
            if (task.execution_time < earliest_time) {
                if (earliest_task != nullptr) {
                    delete earliest_task;
                }
                earliest_time = task.execution_time;
                earliest_task = new Task(std::move(task));
                is_from_periodic = true;
            }
        }

        // 如果最早的任务还没到执行时间，等待
        if (earliest_task && earliest_task->execution_time > now) {
            if (is_from_periodic) {
                periodic_tasks_.front() = std::move(*earliest_task);
            } else {
                once_tasks_.front() = std::move(*earliest_task);
            }
            delete earliest_task;
            cv_.wait_until(lock, earliest_time);
            continue;
        }

        // 执行最早的任务
        if (earliest_task) {
            Task task = std::move(*earliest_task);
            delete earliest_task;

            // 从对应队列中移除任务
            if (is_from_periodic) {
                periodic_tasks_.pop();
                // 重新调度周期性任务
                task.execution_time += task.period;
                periodic_tasks_.push(std::move(task));
            } else {
                once_tasks_.pop();
            }

            lock.unlock();
            task.callback();
        }
    }
}

} // namespace utils