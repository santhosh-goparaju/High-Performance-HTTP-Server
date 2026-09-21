#include "server/thread_pool.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <latch>

using namespace httpserver::server;

TEST(ThreadPoolTest, EnqueueAndExecuteTasks) {
    ThreadPool pool(4, 10);

    std::atomic<int> counter{0};
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(pool.enqueue([&counter]() {
            counter++;
        }));
    }

    pool.shutdown();
    EXPECT_EQ(counter.load(), 5);
}

TEST(ThreadPoolTest, QueueFullReturnsFalse) {
    // Use a latch to block all workers so we can fill the queue
    std::latch gate(1);
    ThreadPool pool(1, 2);

    // Block the single worker
    pool.enqueue([&gate]() {
        gate.wait();
    });
    // Give worker time to pick up the blocking task
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // Now fill the queue (size 2)
    EXPECT_TRUE(pool.enqueue([]() {}));
    EXPECT_TRUE(pool.enqueue([]() {}));

    // Queue should be full now
    EXPECT_FALSE(pool.enqueue([]() {}));

    gate.count_down(); // Release the worker
    pool.shutdown();
}

TEST(ThreadPoolTest, ShutdownWaitsForInFlightTasks) {
    ThreadPool pool(2, 5);

    std::atomic<bool> task_done{false};
    pool.enqueue([&task_done]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        task_done = true;
    });

    // Give the task time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    pool.shutdown();
    EXPECT_TRUE(task_done.load());
}

TEST(ThreadPoolTest, MultipleWorkersProcessConcurrently) {
    ThreadPool pool(4, 20);

    std::atomic<int> active_tasks{0};
    std::atomic<int> max_active{0};
    std::latch start_gate(1); // Ensure all tasks start roughly together

    auto task = [&]() {
        start_gate.wait(); // Wait for all to be queued
        int current = ++active_tasks;
        int expected = max_active.load();
        while (current > expected && !max_active.compare_exchange_weak(expected, current)) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        --active_tasks;
    };

    for (int i = 0; i < 8; ++i) {
        pool.enqueue(task);
    }

    start_gate.count_down(); // Release all tasks
    pool.shutdown();
    EXPECT_GT(max_active.load(), 1);
}

TEST(ThreadPoolTest, ActiveWorkersCount) {
    ThreadPool pool(2, 5);

    std::latch gate(1);
    pool.enqueue([&gate]() {
        gate.wait();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_GT(pool.active_workers(), static_cast<size_t>(0));

    gate.count_down();
    pool.shutdown();
}

TEST(ThreadPoolTest, IsRunningState) {
    ThreadPool pool(2, 5);
    EXPECT_TRUE(pool.is_running());

    pool.shutdown();
    EXPECT_FALSE(pool.is_running());
}

TEST(ThreadPoolTest, EnqueueAfterShutdown) {
    ThreadPool pool(2, 5);
    pool.shutdown();

    EXPECT_FALSE(pool.enqueue([]() {}));
}
