
#include "WorkStealing.hh"
#include "log_utils.h"

WorkStealingThreadPool::WorkStealingThreadPool(size_t threadCount)
    : done(false), rng(random_device{}())
{
    for (size_t i = 0; i < threadCount; ++i)
    {
        queues.emplace_back(make_unique<Queue>());
    }

    for (size_t i = 0; i < threadCount; ++i)
    {
        threads.emplace_back([this, i]
                             { workerLoop(i); });
    }
}

WorkStealingThreadPool::~WorkStealingThreadPool()
{
    done = true; // Signal all worker threads to stop

    for (auto &q : queues)
        q->cv.notify_all(); // Wake up threads that may be waiting for work

    for (auto &t : threads)
        t.join(); // Make sure all threads actually terminate
}

/*
add a task to the thread pool for parallel processing
*/
void WorkStealingThreadPool::enqueue(Task<void> task)
{
    activeTasks++; // increase the count of running tasks

    // Choose a random queue (round-robin or random) in pool to put the task into
    size_t index = dist(rng) % queues.size();
    {
        lock_guard<mutex> lock(queues[index]->mtx);
        queues[index]->tasks.push(std::move(task));
    }

    // Signals to a thread waiting on that queue (if any) that new work has
    // arrived. thread wakes up and starts processing task
    queues[index]->cv.notify_one();
}

void WorkStealingThreadPool::printStatus()
{
    // cout << "[ThreadPool] Active tasks: " << activeTasks.load() << "\n";

    SAFE_COUT("[ThreadPool] Active tasks: " << activeTasks.load());
}

/*
wait until all tasks in the thread pool are complete
*/
void WorkStealingThreadPool::waitAll()
{
    unique_lock<mutex> lock(doneMutex);

    // Only when there are no tasks currently executing (activeTasks == 0)
    // will the thread be awakened and allowed to proceed.
    allDoneCV.wait(lock, [this]
                   { return activeTasks.load() == 0; });
}

// void WorkStealingThreadPool::workerLoop(size_t index)
// {
//     while (!done)
//     {
//         optional<Task<void>> taskOpt; // avoid deleted default constructor
//         bool found = false;

//         for (size_t offset = 0; offset < queues.size(); ++offset)
//         {
//             size_t i = (index + offset) % queues.size();
//             auto &q = *queues[i];
//             unique_lock lock(q.mtx);

//             if (!q.tasks.empty())
//             {
//                 taskOpt = std::move(q.tasks.front());
//                 q.tasks.pop();
//                 found = true;
//                 break;
//             }
//         }

//         if (found && taskOpt.has_value())
//         {
//             taskOpt->handle.resume();
//         }
//         else
//         {
//             auto &q = *queues[index];
//             unique_lock lock(q.mtx);
//             q.cv.wait_for(lock, chrono::milliseconds(100));
//         }
//     }
// }

/*
Controls the work loop of each thread in the WorkStealingThreadPool
*/
void WorkStealingThreadPool::workerLoop(size_t index)
{
    while (!done)
    {
        Task<void> task;

        {
            // Lock the queue corresponding to the thread index
            unique_lock<mutex> lock(queues[index]->mtx);

            // wait when: there is a new task or thread pool is stopped
            queues[index]->cv.wait(lock, [&]
                                   { return done || !queues[index]->tasks.empty(); });

            // If done == true and queue is empty → no more work to do → exit the loop
            if (done && queues[index]->tasks.empty())
                return;

            task = std::move(queues[index]->tasks.front());
            queues[index]->tasks.pop();
        }

        // Wrap the task to reduce the count when completed (attach task counting logic to root task)
        auto wrapped = [this, task = std::move(task)]() mutable -> Task<void>
        {
            co_await task; // wait for the task to actually complete

            if (--activeTasks == 0)
            {
                lock_guard<mutex> lock(doneMutex);
                allDoneCV.notify_all(); // Tell waitAll() that all work is done.
            }

            co_return;
        };

        // Now run the wrapped coroutine properly until completion
        auto wrappedTask = wrapped();

        while (!wrappedTask.handle.done())
        {
            wrappedTask.handle.resume();
        }
    }
}
