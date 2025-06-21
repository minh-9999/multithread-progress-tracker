#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>

using namespace std;

class ThreadPool
{
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    void enqueue(function<void()> job);

private:
    // List of worker threads
    vector<thread> workers;
    // Queue jobs
    queue<function<void()>> jobs;
    mutex queue_mutex;
    condition_variable cv;

    // Stop stream flag
    atomic<bool> stop;
};
