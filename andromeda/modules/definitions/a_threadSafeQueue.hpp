#pragma once
#include <queue>
#include <mutex>
#include <vector>
#include <stdexcept>
namespace Andromeda{
    template<typename T>
    class ThreadSafeQueue {
    public:
       ThreadSafeQueue() = default;
       void push(T value) { 
           std::lock_guard lock(m_Mutex); 
           m_Queue.push(std::move(value)); 
       }

       T dequeue() { 
           std::lock_guard lock(m_Mutex);
           if (!m_Queue.empty()) {
               auto value = std::move(m_Queue.front());
               m_Queue.pop();
               return value;
           }
           throw std::runtime_error("thread safe queue is empty");
       }

       std::vector<T> dequeueAll() {
           std::lock_guard lock(m_Mutex);
           std::vector<T> values;

           values.reserve(m_Queue.size());

           while (!m_Queue.empty()) {
               values.push_back(std::move(m_Queue.front()));
               m_Queue.pop();
           }
           return values;
       }

       bool empty() const {
           std::lock_guard lock(m_Mutex);
           return m_Queue.empty();
       }

       size_t size() const {
           std::lock_guard lock(m_Mutex);
           return m_Queue.size();
       }
    private:
        std::queue<T> m_Queue;
        mutable std::mutex m_Mutex;
    };
}