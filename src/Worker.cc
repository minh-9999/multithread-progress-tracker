#include "Worker.hh"
#include "JobExecutor.hh"

#include <memory>

Worker::Worker(LockFreeDeque<Job> &localQueue, vector<LockFreeDeque<Job> *> &all)
    : queues(localQueue), allQueues(all)
{
    start();
}

void Worker::start()
{
    threads = thread(&Worker::run, this);
}

void Worker::stop()
{
    running = false;
}

void Worker::join()
{
    if (threads.joinable())
        threads.join();
}

void Worker::run()
{
    while (running)
    {
        unique_ptr<Job> job;
        if (queues.popBottom(job) || steal(job))
        {
            JobExecutor::run(std::move(job));
        }
        else
        {
            this_thread::yield();
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }
}

bool Worker::steal(unique_ptr<Job> &job)
{
    for (auto &q : allQueues)
    {
        if (q == &queues)
            continue;

        if (q->stealTop(job))
            return true;
    }
    return false;
}
