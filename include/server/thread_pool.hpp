#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <cstddef>

namespace httpserver::server {

class ThreadPool {
public:
    ThreadPool(int num_workers, size_t max_queue_size);
    ~ThreadPool();
    
    // Move-only semantics
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    bool enqueue(std::function<void()> task);
    void shutdown();
    
    size_t queue_size() const;
    size_t active_workers() const;
    bool is_running() const;

private:
    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_;
    std::atomic<size_t> active_count_;
    size_t max_queue_size_;

    void worker_loop(std::stop_token stoken);
};

} // namespace httpserver::server
