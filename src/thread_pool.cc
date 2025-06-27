
#include "thread_pool.hh"

ThreadPool::ThreadPool(size_t numThreads) : stop(false)
{
    // create numThreads new thread
    for (size_t i = 0; i < numThreads; ++i)
    {
        workers.emplace_back([this]()
                             {
            while (true) 
            {
                function<void()> task;

                {
                    unique_lock lock(queue_mutex);
                    cv.wait(lock, [this]() { return stop || !jobs.empty(); });

                    // If stopped and no more work → exit thread
                    if (stop && jobs.empty()) 
                        return;

                    task = std::move(jobs.front()); // Take the job out of the queue
                    jobs.pop();
                }

                task();
            } });
    }
}

void ThreadPool::enqueue(function<void()> job)
{
    {
        lock_guard lock(queue_mutex);
        jobs.emplace([job, this]()
                     {
                         job(); // Run coroutine
                         lock_guard lock(completion_mutex);
                         completed_jobs.push_back(this_thread::get_id()); // Track completed thread
                     });
    }

    cv.notify_one(); // 🔔 Wake up an idle thread
}

ThreadPool::~ThreadPool()
{
    stop = true;     // 🛑 Set stop flag
    cv.notify_all(); // 🔔 Wake up all sleeping threads

    for (thread &worker : workers)
    {
        if (worker.joinable()) // ✅ Check if thread is still running
            worker.join();     // 🔚 Wait for thread to end
    }
}