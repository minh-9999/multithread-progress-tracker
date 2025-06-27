#pragma once

#include "task.hh"
#include <functional>
#include <queue>
#include <unordered_map>

class Thread_Pool
{
public:
    Thread_Pool(size_t num_threads);

    template <typename F>
    void enqueue(F &&task)
    {
        {
            std::unique_lock lock(queue_mutex);
            tasks.push(std::forward<F>(task));
        }

        condition.notify_one();
    }

    ~Thread_Pool();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
};

struct TaskGraph
{
    // std::vector<Task<void>> tasks;
    std::vector<std::shared_ptr<Task<void>>> tasks; // List of Tasks to manage coroutines
    Thread_Pool pool;

    TaskGraph(size_t num_threads) ;

    void addTask(std::shared_ptr<Task<void>> task);

    bool has_cycle();

    bool detect_cycle(Task<void> *task, std::unordered_map<Task<void> *, int> &visited);

    void execute();

    void wait_all();
};



