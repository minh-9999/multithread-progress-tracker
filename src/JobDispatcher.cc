#include "JobDispatcher.hh"

JobDispatcher::JobDispatcher(int n) : numThreads(n)
{
    // Create job queues
    for (int i = 0; i < numThreads; ++i)
        queues.emplace_back(make_unique<LockFreeDeque<Job>>());

    // Create Workers
    for (int i = 0; i < numThreads; ++i)
        workers.emplace_back(make_unique<Worker>(*queues[i], getAllQueues()));
}

// dispatch (distribute) a job to a specific worker's queue, based on the threadIndex
void JobDispatcher::dispatch(int threadIndex, unique_ptr<Job> job)
{
    // Check valid index
    if (threadIndex < 0 || threadIndex >= static_cast<int>(queues.size()))
    {
        throw out_of_range("Invalid threadIndex in dispatch()");
    }

    // Put jobs in queue
    queues[threadIndex]->pushBottom(std::move(job));
}

void JobDispatcher::stop()
{
    // Send stop signal to each worker
    for (auto &w : workers)
        w->stop();

    // Wait for each worker to complete and end the thread
    for (auto &w : workers)
        w->join();
}

vector<LockFreeDeque<Job> *> &JobDispatcher::getAllQueues()
{
    static vector<LockFreeDeque<Job> *> all;

    if (all.empty())
    {
        for (auto &q : queues)
            all.push_back(q.get()); // get raw pointer (LockFreeDeque<Job>*) from unique_ptr
    }

    return all;
}
