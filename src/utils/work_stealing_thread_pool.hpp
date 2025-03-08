#pragma once
#include <vector>
#include <deque>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <functional>
#include <future>
#include <random>

namespace utils {

class WorkStealingThreadPool {
public:
    explicit WorkStealingThreadPool(size_t num_threads);
    ~WorkStealingThreadPool();

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type>;

private:
    struct TaskQueue {
        std::deque<std::function<void()>> tasks;
        std::mutex mutex;
        std::condition_variable condition;
    };

    std::vector<std::thread> workers;
    std::vector<std::unique_ptr<TaskQueue>> local_queues;
    std::atomic<bool> stop;
    std::atomic<size_t> next_queue_index{0};
    
    bool steal_task(std::function<void()>& task, size_t current_index);
    size_t get_random_queue_index(size_t max_index, size_t current_index);
};

// Template implementation
template<class F, class... Args>
auto WorkStealingThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result<F, Args...>::type>
{
    using return_type = typename std::invoke_result<F, Args...>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
        
    std::future<return_type> res = task->get_future();
    
    // 使用轮询方式分配任务到不同队列
    size_t queue_index = next_queue_index++ % local_queues.size();
    
    {
        auto& queue = local_queues[queue_index];
        std::unique_lock<std::mutex> lock(queue->mutex);
        queue->tasks.emplace_back([task](){ (*task)(); });
    }
    local_queues[queue_index]->condition.notify_one();
    
    return res;
}

} // namespace utils