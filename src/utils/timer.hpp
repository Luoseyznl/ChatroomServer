#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace utils {

/**
 * @brief 定时器类，支持一次性任务和周期性任务
 *
 * 可以安排任务在未来某个时间点执行，或以固定的时间间隔重复执行。
 */
class Timer {
 public:
  /**
   * @brief 定义一个任务结构体
   *
   * 包含任务的执行时间、回调函数、是否为周期性任务及其周期。
   */
  struct Task {
    std::chrono::steady_clock::time_point execution_time;
    std::function<void()> callback;
    bool is_periodic;
    std::chrono::milliseconds period;

    Task(std::chrono::steady_clock::time_point exec_time,
         std::function<void()> cb, bool periodic = false,
         std::chrono::milliseconds period_duration =
             std::chrono::milliseconds(0))
        : execution_time(exec_time),
          callback(std::move(cb)),
          is_periodic(periodic),
          period(period_duration) {}
    bool operator>(const Task& other) const {
      return execution_time > other.execution_time;
    }
  };

  explicit Timer();
  ~Timer();
  // 一次性任务，将在当前时间点之后的 delay 执行一次
  void addOnceTask(std::chrono::milliseconds delay,
                   std::function<void()> callback);
  // 周期性任务，将在当前时间点之后的 delay 执行，并每隔 period 执行一次
  void addPeriodicTask(std::chrono::milliseconds delay,
                       std::chrono::milliseconds period,
                       std::function<void()> callback);
  void start();
  void stop();

 private:
  std::priority_queue<Task, std::vector<Task>, std::greater<Task>> task_queue_;
  std::mutex queue_mutex_;
  std::condition_variable condition_;
  std::thread timer_thread_;
  bool running_;
};
}  // namespace utils

#endif  // TIMER_HPP
