#pragma once
#include <deque>
#include <mutex>

// Simplified version, not truly lock-free yet
// You can later replace this with a real lock-free deque (e.g., Chase-Lev)
template <typename T>
class LockFreeDeque
{
private:
    // std::deque<T> deques;
    std::mutex mutex;
    std::deque<std::unique_ptr<T>> deques;

public:
    void pushBottom(std::unique_ptr<T> &&jobPtr)
    {
        std::lock_guard<std::mutex> lock(mutex);
        deques.push_back(std::move(jobPtr));
    }

    bool popBottom(std::unique_ptr<T> &jobPtr)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (deques.empty())
            return false;

        jobPtr = std::move(deques.back());
        deques.pop_back();
        return true;
    }

    bool stealTop(std::unique_ptr<T> &jobPtr)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (deques.empty())
            return false;

        jobPtr = std::move(deques.front());
        deques.pop_front();
        return true;
    }
};
