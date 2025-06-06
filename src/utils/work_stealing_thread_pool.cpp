#include "work_stealing_thread_pool.hpp"

#include <random>

namespace utils {

WorkStealingThreadPool::WorkStealingThreadPool(size_t num_threads)
    : stop_(false) {
  // 1 为每个线程创建本地任务队列
  local_queues_.resize(num_threads);
  for (size_t i = 0; i < num_threads; ++i) {
    local_queues_[i] = std::make_unique<TaskQueue>();
  }

  // 2 创建并启动工作线程
  for (size_t i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this, i] {
      while (true) {
        std::function<void()> task;
        bool has_task = false;

        // 尝试从本地队列获取任务
        {
          auto& local_queue = local_queues_[i];
          std::unique_lock<std::mutex> lock(local_queue->mutex);
          if (!local_queue->tasks.empty()) {
            task = std::move(local_queue->tasks.front());
            local_queue->tasks.pop_front();
            has_task = true;
          }
        }

        // 本地队列为空时主动从其他队列窃取任务
        if (!has_task) {
          if (steal_task(task, i)) {
            has_task = true;
          } else {
            // 如果没有任务可窃取，等待新任务
            auto& local_queue = local_queues_[i];
            std::unique_lock<std::mutex> lock(local_queue->mutex);
            local_queue->condition.wait(lock, [this, &local_queue] {
              return stop_ || !local_queue->tasks.empty();
            });

            if (stop_ && local_queue->tasks.empty()) {
              return;
            }

            if (!local_queue->tasks.empty()) {
              task = std::move(local_queue->tasks.front());
              local_queue->tasks.pop_front();
              has_task = true;
            }
          }
        }

        if (has_task) {
          task();
        }
      }
    });
  }
}

WorkStealingThreadPool::~WorkStealingThreadPool() {
  // 设置停止标志、通知所有线程、阻塞等待结束
  stop_ = true;
  for (auto& queue : local_queues_) {
    queue->condition.notify_all();
  }
  for (std::thread& worker : workers_) {
    worker.join();
  }
}

bool WorkStealingThreadPool::steal_task(std::function<void()>& task,
                                        size_t current_index) {
  size_t num_queues = local_queues_.size();
  size_t start_index = get_random_queue_index(num_queues, current_index);

  for (size_t i = 0; i < num_queues - 1; ++i) {
    size_t index = (start_index + i) % num_queues;
    if (index == current_index) continue;  // 跳过当前线程的队列

    auto& queue = local_queues_[index];
    // 尝试非阻塞地获取锁
    std::unique_lock<std::mutex> lock(queue->mutex, std::try_to_lock);
    if (lock && !queue->tasks.empty()) {
      task = std::move(queue->tasks.front());
      queue->tasks.pop_front();
      return true;
    }
  }
  return false;
}

size_t WorkStealingThreadPool::get_random_queue_index(size_t max_index,
                                                      size_t current_index) {
  static thread_local std::mt19937 gen(std::random_device{}());
  std::uniform_int_distribution<size_t> dist(0, max_index - 1);
  size_t index;
  do {
    index = dist(gen);
  } while (index == current_index && max_index > 1);
  return index;
}

}  // namespace utils