#include "thread_pool.hpp"

#include <algorithm>
#include <random>

namespace utils {
ThreadPool::ThreadPool(size_t num_threads) {
  stop_.store(false);
  task_queues_.resize(num_threads);
  for (size_t i = 0; i < num_threads; ++i) {
    task_queues_[i] = std::make_unique<TaskQueue>();
  }

  for (size_t i = 0; i < num_threads; ++i) {
    workers_.emplace_back([this, i]() {
      while (!stop_.load()) {
        std::function<void()> task;
        bool has_task = false;
        {
          auto& task_queue = task_queues_[i];
          std::unique_lock<std::mutex> lock(task_queue->mutex);
          if (!task_queue->tasks.empty()) {
            task = std::move(task_queue->tasks.front());
            task_queue->tasks.pop_front();
            has_task = true;
          }
        }

        if (!has_task) {
          if (steal_task(i, task)) {
            has_task = true;
          } else {
            auto& task_queue = task_queues_[i];
            std::unique_lock<std::mutex> lock(task_queue->mutex);
            task_queue->condition.wait(lock, [this, &task_queue] {
              return stop_.load() || !task_queue->tasks.empty();
            });
            if (stop_.load() && task_queue->tasks.empty()) {
              return;
            }
            if (!task_queue->tasks.empty()) {
              task = std::move(task_queue->tasks.front());
              task_queue->tasks.pop_front();
              has_task = true;
            }
          }
        }

        if (has_task) {
          task();
        } else {
          std::this_thread::yield();
        }
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  stop_.store(true);
  for (auto& task_queue : task_queues_) {
    task_queue->condition.notify_all();
  }
  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

bool ThreadPool::steal_task(size_t thread_index, std::function<void()>& task) {
  size_t num_queues = task_queues_.size();
  std::vector<size_t> indices;
  indices.reserve(num_queues - 1);
  for (size_t i = 0; i < num_queues; ++i) {
    if (i != thread_index) {
      indices.push_back(i);
    }
  }
  static thread_local std::random_device rd;
  static thread_local std::mt19937 gen(rd());
  std::shuffle(indices.begin(), indices.end(), gen);

  for (size_t index : indices) {
    auto& task_queue = task_queues_[index];
    std::unique_lock<std::mutex> lock(task_queue->mutex);
    if (!task_queue->tasks.empty()) {
      task = std::move(task_queue->tasks.front());
      task_queue->tasks.pop_front();
      return true;
    }
  }
  return false;
}
}  // namespace utils
