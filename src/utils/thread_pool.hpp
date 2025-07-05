#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace utils {
class ThreadPool {
 public:
  explicit ThreadPool(size_t num_threads);
  ~ThreadPool();

  template <typename F, typename... Args>
  auto enqueue(F&& f, Args&&... args)
      -> std::future<typename std::invoke_result<F, Args...>::type>;

 private:
  struct TaskQueue {
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<std::function<void()>> tasks;
  };

  std::vector<std::thread> workers_;
  std::vector<std::unique_ptr<TaskQueue>> task_queues_;
  std::atomic<bool> stop_;
  std::atomic<size_t> queue_index_{0};

  bool steal_task(size_t thread_index, std::function<void()>& task);
};

template <typename F, typename... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type> {
  using return_type = typename std::invoke_result<F, Args...>::type;
  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));
  std::future<return_type> result = task->get_future();

  size_t queue_index = queue_index_++ % workers_.size();
  {
    std::lock_guard<std::mutex> lock(task_queues_[queue_index]->mutex);
    task_queues_[queue_index]->tasks.emplace_back([task]() { (*task)(); });
  }
  task_queues_[queue_index]->condition.notify_one();
  return result;
}
}  // namespace utils
