#include "timer.hpp"

#include <iostream>

namespace utils {

Timer::Timer() : running_(false) {}
Timer::~Timer() { stop(); }

void Timer::addOnceTask(std::chrono::milliseconds delay,
                        std::function<void()> callback) {
  auto execution_time = std::chrono::steady_clock::now() + delay;
  Task task(execution_time, std::move(callback));

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    task_queue_.push(task);
    condition_.notify_one();
  }
}

void Timer::addPeriodicTask(std::chrono::milliseconds delay,
                            std::chrono::milliseconds period,
                            std::function<void()> callback) {
  auto execution_time = std::chrono::steady_clock::now() + delay;
  Task task(execution_time, std::move(callback), true, period);
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    task_queue_.push(task);
    condition_.notify_one();
  }
}

void Timer::stop() {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    running_ = false;
    condition_.notify_all();  // Notify the thread to wake up and exit
  }

  if (timer_thread_.joinable()) {
    timer_thread_.join();
  }
}

/**
 * @brief 启动定时器
 *
 * 创建后台线程处理定时任务队列
 */
void Timer::start() {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (running_) return;
    running_ = true;
  }

  timer_thread_ = std::thread([this]() {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    while (running_) {
      if (task_queue_.empty()) {
        condition_.wait(lock,
                        [this]() { return !running_ || !task_queue_.empty(); });
      } else {
        auto now = std::chrono::steady_clock::now();
        auto next_task = task_queue_.top();

        if (next_task.execution_time <= now) {
          // 任务到期，执行回调
          task_queue_.pop();
          if (next_task.is_periodic) {
            auto next_execution_time =
                next_task.execution_time + next_task.period;

            Task periodic_task(next_execution_time, next_task.callback, true,
                               next_task.period);
            task_queue_.push(periodic_task);
          }

          try {
            lock.unlock();  // 回调前解锁，避免死锁
            next_task.callback();
          } catch (const std::exception& e) {
            std::cerr << "Exception in task callback: " << e.what()
                      << std::endl;
          }
          lock.lock();

        } else {
          // 任务未到期，等待下一个任务到期、新任务入队或定时器停止
          condition_.wait_until(
              lock, next_task.execution_time, [this, &next_task]() {
                return !running_ || (!task_queue_.empty() &&
                                     task_queue_.top().execution_time <=
                                         next_task.execution_time);
              });
        }
      }
    }
  });
}

}  // namespace utils