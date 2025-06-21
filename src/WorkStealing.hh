#pragma once

#include "task.hh"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <random>
#include <thread>

using namespace std;

template <typename F>
Task<void> wrapAsTask(F &&f)
{
    f();       // Call function F immediately
    co_return; // Returns a coroutine that does nothing (of type Task<void>)
}

class WorkStealingThreadPool
{
public:
    explicit WorkStealingThreadPool(size_t threadCount = thread::hardware_concurrency());
    ~WorkStealingThreadPool();

    // Enqueue a Task<F> into the pool, then convert it to a Task<void>
    template <typename F>
    void enqueue(Task<F> task)
    {
        enqueue(wrapAsTask(std::move(task)));
    }

    void enqueue(Task<void> task);

    void printStatus();

    void waitAll();

private:
    struct Queue
    {
        mutex mtx;
        condition_variable cv;
        queue<Task<void>> tasks; // Queue containing pending tasks
    };

    vector<unique_ptr<Queue>> queues; // Each worker thread has its own queue
    vector<thread> threads;           // List of worker threads
    atomic<bool> done;                // Mark if pool has been requested to stop
    mt19937 rng;                      // Random number generator for queue selection
    uniform_int_distribution<> dist;  // Random distribution to select queue

    atomic<int> activeTasks{0}; // count the number of active tasks
    // condition_variable_any allDoneCV;
    condition_variable allDoneCV; // Notified when `activeTasks == 0`
    mutex doneMutex;              // Protect `allDoneCV` when waiting or notifying

    void workerLoop(size_t index);
};