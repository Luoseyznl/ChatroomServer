#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

/**
 * 工作窃取线程池设计原理：
 *
 * 1. 分布式任务队列
 *    - 每个工作线程拥有自己的本地任务队列（独占指针队列），减少全局锁争用
 *    - 提交时通过轮询方式分配到各线程队列（以共享指针存储，允许多个线程访问）
 *
 * 2. 工作窃取机制
 *    - 当线程自己的队列为空时，会随机尝试从其他线程队列"窃取"任务
 *    - 窃取操作使用非阻塞锁(try_lock)，避免长时间等待
 *
 * 3. 高效的负载均衡
 *    - 忙碌的线程(队列非空)专注处理自己的任务
 *    - 空闲的线程主动寻找工作，实现动态负载均衡
 *    - 减少了线程间等待，提高了CPU利用率
 */

namespace utils {

class WorkStealingThreadPool {
 public:
  explicit WorkStealingThreadPool(size_t num_threads);
  ~WorkStealingThreadPool();

  template <class F, class... Args>
  auto enqueue(F&& f, Args&&... args)
      -> std::future<typename std::invoke_result<F, Args...>::type>;

 private:
  /**
   * @brief 任务队列结构体
   *
   * 每个工作线程都有专属的任务队列、互斥锁、条件变量
   */
  struct TaskQueue {
    std::deque<std::function<void()>> tasks;
    std::mutex mutex;
    std::condition_variable condition;
  };

  std::vector<std::thread> workers_;
  std::vector<std::unique_ptr<TaskQueue>> local_queues_;
  std::atomic<bool> stop_;
  std::atomic<size_t> next_queue_index{0};

  bool steal_task(std::function<void()>& task, size_t current_index);
  size_t get_random_queue_index(size_t max_index, size_t current_index);
};

// Template implementation
template <class F, class... Args>
auto WorkStealingThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::invoke_result<F, Args...>::type> {
  using return_type = typename std::invoke_result<F, Args...>::type;

  auto task = std::make_shared<std::packaged_task<return_type()>>(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...));

  std::future<return_type> res = task->get_future();

  // 使用轮询方式分配任务到不同队列
  size_t queue_index = next_queue_index++ % local_queues_.size();

  {
    auto& queue = local_queues_[queue_index];
    std::unique_lock<std::mutex> lock(queue->mutex);
    queue->tasks.emplace_back([task]() { (*task)(); });
  }
  local_queues_[queue_index]->condition.notify_one();

  return res;
}

}  // namespace utils