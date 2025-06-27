#pragma once

#include "task_graph.hh"
#include "../third_party/asio-src/asio/include/asio.hpp"

class AsyncIOContext
{
public:
    AsyncIOContext() : io_context(), work_guard(asio::make_work_guard(io_context))
    {
        worker_thread = std::thread([this]() { io_context.run(); });
    }

    asio::io_context &get_io_context()
    {
        return io_context;
    }

    ~AsyncIOContext()
    {
        io_context.stop();
        if (worker_thread.joinable())
            worker_thread.join();
    }

private:
    asio::io_context io_context;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard;
    std::thread worker_thread;
};

template <typename T>
struct AsyncTask : public Task<T>
{
    static AsyncIOContext io_context; // Manage Async IO

    void async_wait_for_io(std::chrono::milliseconds duration)
    {
        auto timer = std::make_shared<asio::steady_timer>(io_context.get_io_context(), duration);

        timer->async_wait([this, timer](const asio::error_code &)
                          { this->handle.resume(); });
    }
};

struct AsyncTaskGraph : public TaskGraph
{
    void execute()
    {
        std::unordered_map<Task<void> *, size_t> dependency_count;
        std::queue<Task<void> *> ready_tasks;

        for (auto &task : tasks)
        {
            size_t count = task->dependencies.size();
            // dependency_count[&task] = count;
            dependency_count[task.get()] = count;

            if (count == 0)
                // ready_tasks.push(&task);
                ready_tasks.push(task.get());
        }

        while (!ready_tasks.empty())
        {
            Task<void> *task = ready_tasks.front();
            ready_tasks.pop();

            if (auto async_task = static_cast<AsyncTask<void> *>(task))
            {
                async_task->async_wait_for_io(std::chrono::milliseconds(500)); // Wait for simulated I/O
            }
            else
            {
                pool.enqueue([task]
                             { task->execute(); });
            }

            for (auto &next_task : tasks)
            {
                if (std::find(next_task->dependencies.begin(), next_task->dependencies.end(), *task) != next_task->dependencies.end())
                {
                    // if (--dependency_count[&next_task] == 0)
                    // ready_tasks.push(&next_task);

                    if (--dependency_count[next_task.get()] == 0)
                        ready_tasks.push(next_task.get());
                }
            }
        }
    }
};


