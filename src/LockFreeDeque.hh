#pragma once
#include <deque>
#include <mutex>

// Simplified version, not truly lock-free yet
// You can later replace this with a real lock-free deque (e.g., Chase-Lev)
template <typename T>
class LockFreeDeque
{
private:
    std::deque<T> deque;
    std::mutex mutex;

public:
    void pushBottom(const T &job)
    {
        std::lock_guard<std::mutex> lock(mutex);
        deque.push_back(job);
    }

    bool popBottom(T &job)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (deque.empty())
            return false;

        job = deque.back();
        deque.pop_back();
        return true;
    }

    bool stealTop(T &job)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (deque.empty())
            return false;

        job = deque.front();
        deque.pop_front();
        return true;
    }
};
