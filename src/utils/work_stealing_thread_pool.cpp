#include "work_stealing_thread_pool.hpp"
#include <random>

namespace utils {

WorkStealingThreadPool::WorkStealingThreadPool(size_t num_threads) : stop(false) {
    local_queues.resize(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        local_queues[i] = std::make_unique<TaskQueue>();
    }

    for(size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this, i] {
            while(true) {
                std::function<void()> task;
                bool has_task = false;
                
                // 首先尝试从本地队列获取任务
                {
                    auto& local_queue = local_queues[i];
                    std::unique_lock<std::mutex> lock(local_queue->mutex);
                    if (!local_queue->tasks.empty()) {
                        task = std::move(local_queue->tasks.front());
                        local_queue->tasks.pop_front();
                        has_task = true;
                    }
                }

                // 如果本地队列为空，尝试从其他队列窃取任务
                if (!has_task) {
                    if (steal_task(task, i)) {
                        has_task = true;
                    } else {
                        // 如果没有任务可窃取，等待新任务
                        auto& local_queue = local_queues[i];
                        std::unique_lock<std::mutex> lock(local_queue->mutex);
                        local_queue->condition.wait(lock, [this, &local_queue] {
                            return stop || !local_queue->tasks.empty();
                        });
                        
                        if (stop && local_queue->tasks.empty()) {
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
    stop = true;
    for (auto& queue : local_queues) {
        queue->condition.notify_all();
    }
    for(std::thread &worker: workers) {
        worker.join();
    }
}

bool WorkStealingThreadPool::steal_task(std::function<void()>& task, size_t current_index) {
    size_t num_queues = local_queues.size();
    size_t start_index = get_random_queue_index(num_queues, current_index);
    
    // 尝试从其他队列窃取任务
    for (size_t i = 0; i < num_queues - 1; ++i) {
        size_t index = (start_index + i) % num_queues;
        if (index == current_index) continue; // 跳过当前线程的队列
        
        auto& queue = local_queues[index];
        std::unique_lock<std::mutex> lock(queue->mutex, std::try_to_lock);
        if (lock && !queue->tasks.empty()) {
            task = std::move(queue->tasks.front());
            queue->tasks.pop_front();
            return true;
        }
    }
    return false;
}

size_t WorkStealingThreadPool::get_random_queue_index(size_t max_index, size_t current_index) {
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<size_t> dist(0, max_index - 1);
    size_t index;
    do {
        index = dist(gen);
    } while (index == current_index && max_index > 1);
    return index;
}

} // namespace utils