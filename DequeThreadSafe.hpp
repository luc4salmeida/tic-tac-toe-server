#pragma once

#include <deque>
#include <mutex>

template <typename T>
class DequeThreadSafe {
public:
    DequeThreadSafe() = default;
    ~DequeThreadSafe() = default;

public:
    DequeThreadSafe(const DequeThreadSafe&) = delete;
    DequeThreadSafe& operator=(const DequeThreadSafe&) = delete;

public:
    T& front() {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_deque.empty()) {
            throw std::runtime_error("Deque is empty");
        }

        return m_deque.front();
    }

    T& back() {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_deque.empty()) {
            throw std::runtime_error("Deque is empty");
        }

        return m_deque.back();
    }

    void push_back(const T& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_deque.push_back(value);
    }

    T pop_front() {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_deque.empty()) {
            throw std::runtime_error("Deque is empty");
        }

        T value = m_deque.front();
        m_deque.pop_front();
        return value;
    }

    bool is_empty() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_deque.empty();
    }

private:
    std::deque<T> m_deque;
    std::mutex m_mutex;
};
