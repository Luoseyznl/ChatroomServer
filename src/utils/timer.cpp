#include "timer.hpp"

#include <iostream>

namespace utils {

Timer::Timer() : running_(false) {}
Timer::~Timer() { stop(); }

void Timer::addOnceTask(std::chrono::milliseconds delay,
                        std::function<void()> callback) {
  auto execution_time = std::chrono::steady_clock::now() + delay;
  TimerTask task(execution_time, std::move(callback));

  {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    taskQueue_.push(task);
    tasksCv_.notify_one();
  }
}

void Timer::addPeriodicTask(std::chrono::milliseconds delay,
                            std::chrono::milliseconds period,
                            std::function<void()> callback) {
  auto execution_time = std::chrono::steady_clock::now() + delay;
  TimerTask task(execution_time, std::move(callback), true, period);
  {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    taskQueue_.push(task);
    tasksCv_.notify_one();
  }
}

void Timer::start() {
  {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    if (running_) return;  // Already running
    running_ = true;
  }

  timerThread_ = std::thread([this]() {
    std::unique_lock<std::mutex> lock(tasksMutex_);
    while (running_) {
      if (taskQueue_.empty()) {
        tasksCv_.wait(lock,
                      [this]() { return !taskQueue_.empty() || !running_; });
      } else {
        auto now = std::chrono::steady_clock::now();
        auto nextTask = taskQueue_.top();

        if (nextTask.execTimestamp <= now) {
          taskQueue_.pop();

          if (nextTask.isPeriodic) {
            nextTask.execTimestamp += nextTask.period;
            taskQueue_.push(nextTask);
          }

          lock.unlock();  // Unlock before executing the callback
          nextTask.callback();
          lock.lock();
        } else {
          tasksCv_.wait_until(
              lock, nextTask.execTimestamp, [this, &nextTask]() {
                return !running_ ||
                       (!taskQueue_.empty() && taskQueue_.top().execTimestamp <=
                                                   nextTask.execTimestamp);
              });
        }
      }
    }
  });
}

void Timer::stop() {
  {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    running_ = false;
    tasksCv_.notify_all();  // Notify the thread to wake up and exit
  }

  if (timerThread_.joinable()) {
    timerThread_.join();
  }
}

}  // namespace utils
