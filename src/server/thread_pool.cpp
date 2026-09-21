#include "server/thread_pool.hpp"

namespace httpserver::server {

ThreadPool::ThreadPool(int num_workers, size_t max_queue_size)
    : stop_(false), active_count_(0), max_queue_size_(max_queue_size) {
    for (int i = 0; i < num_workers; ++i) {
        workers_.emplace_back([this](std::stop_token stoken) {
            worker_loop(stoken);
        });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

bool ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stop_) return false;
        if (tasks_.size() >= max_queue_size_) return false;
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
    return true;
}

void ThreadPool::shutdown() {
    if (!stop_.exchange(true)) {
        cv_.notify_all();
        // jthread destructor requests stop and joins automatically
        workers_.clear();
    }
}

size_t ThreadPool::queue_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

size_t ThreadPool::active_workers() const {
    return active_count_.load();
}

bool ThreadPool::is_running() const {
    return !stop_.load();
}

void ThreadPool::worker_loop(std::stop_token stoken) {
    while (!stoken.stop_requested()) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this, &stoken] { 
                return stop_ || !tasks_.empty() || stoken.stop_requested(); 
            });
            
            if ((stop_ || stoken.stop_requested()) && tasks_.empty()) {
                return;
            }
            
            if (tasks_.empty()) continue;
            
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        
        active_count_++;
        try {
            task();
        } catch (...) {
            // Exceptions caught at thread boundary
        }
        active_count_--;
    }
}

} // namespace httpserver::server
