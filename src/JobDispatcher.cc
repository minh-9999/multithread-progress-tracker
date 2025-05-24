#include "JobDispatcher.hh"

JobDispatcher::JobDispatcher(int n) : numThreads(n)
{
    for (int i = 0; i < numThreads; ++i)
        queues.emplace_back(std::make_unique<LockFreeDeque<Job>>());

    for (int i = 0; i < numThreads; ++i)
        workers.emplace_back(std::make_unique<Worker>(*queues[i], getAllQueues()));
}

void JobDispatcher::dispatch(int threadIndex, unique_ptr<Job> job)
{
    queues[threadIndex]->pushBottom(std::move(job));
}

void JobDispatcher::stop()
{
    for (auto &w : workers)
        w->stop();

    for (auto &w : workers)
        w->join();
}

std::vector<LockFreeDeque<Job> *> &JobDispatcher::getAllQueues()
{
    static std::vector<LockFreeDeque<Job> *> all;

    if (all.empty())
    {
        for (auto &q : queues)
            all.push_back(q.get());
    }

    return all;
}
