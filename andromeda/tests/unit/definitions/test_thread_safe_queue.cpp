// ThreadSafeQueue hands sensor messages and file events from worker threads to the main thread.

#include <gtest/gtest.h>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>
#include "a_threadSafeQueue.hpp"

using Andromeda::ThreadSafeQueue;

TEST(ThreadSafeQueue, DeliversInTheOrderPushed) {
    ThreadSafeQueue<int> queue;
    for (int i = 0; i < 5; ++i) {
        queue.push(i);
    }

    EXPECT_EQ(queue.dequeue(), 0);
    EXPECT_EQ(queue.dequeueAll(), (std::vector<int>{ 1, 2, 3, 4 }));
    EXPECT_TRUE(queue.empty());
}

TEST(ThreadSafeQueue, DequeueOnAnEmptyQueueThrows) {
    // dequeue() does not wait for data - callers check empty() or use dequeueAll().
    ThreadSafeQueue<int> queue;

    EXPECT_THROW((void)queue.dequeue(), std::runtime_error);
}

TEST(ThreadSafeQueue, ConcurrentProducersLoseAndDuplicateNothing) {
    constexpr int producers = 4;
    constexpr int perProducer = 20000;
    ThreadSafeQueue<int> queue;
    std::atomic<int> running = producers;

    std::vector<std::thread> threads;
    for (int p = 0; p < producers; ++p) {
        threads.emplace_back([&queue, &running, p] {
            for (int i = 0; i < perProducer; ++i) {
                queue.push(p * perProducer + i);
            }
            --running;
        });
    }
    // The consumer drains while the producers are still pushing, like the main thread does.
    std::vector<int> received;
    while (running > 0 || !queue.empty()) {
        for (const int value : queue.dequeueAll()) {
            received.push_back(value);
        }
    }
    for (std::thread& thread : threads) {
        thread.join();
    }

    ASSERT_EQ(received.size(), static_cast<size_t>(producers * perProducer));
    std::vector<bool> seen(received.size(), false);
    for (const int value : received) {
        ASSERT_FALSE(seen[value]) << value << " arrived twice";
        seen[value] = true;
    }
    // Each producer's values arrive in the order it pushed them.
    std::vector<int> lastOfProducer(producers, -1);
    for (const int value : received) {
        const int producer = value / perProducer;
        EXPECT_GT(value, lastOfProducer[producer]);
        lastOfProducer[producer] = value;
    }
}
