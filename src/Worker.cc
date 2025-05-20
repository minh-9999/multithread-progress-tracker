#include "Worker.hh"

Worker::Worker(LockFreeDeque<Job> &localQueue, std::vector<LockFreeDeque<Job> *> &all)
    : queue(localQueue), allQueues(all)
{
    start();
}

void Worker::start()
{
    thread = std::thread(&Worker::run, this);
}

void Worker::stop()
{
    running = false;
}

void Worker::join()
{
    if (thread.joinable())
        thread.join();
}

void Worker::run()
{
    while (running)
    {
        Job job;
        if (queue.popBottom(job) || steal(job))
        {
            job.execute();
        }
        else
        {
            std::this_thread::yield();
        }
    }
}

bool Worker::steal(Job &job)
{
    for (auto &q : allQueues)
    {
        if (q == &queue)
            continue;
        if (q->stealTop(job))
            return true;
    }
    return false;
}
