#pragma once

#include "LockFreeDeque.hh"
#include "Worker.hh"
#include "Job.hh"

class JobDispatcher
{
public:
    explicit JobDispatcher(int n);
    void dispatch(int threadIndex, unique_ptr<Job> job);
    void stop();

private:
    // Job queue list, each thread (worker) has its own queue
    vector<unique_ptr<LockFreeDeque<Job>>> queues;
    // List of workers (threads that process work)
    vector<unique_ptr<Worker>> workers;
    // Number of workers (and corresponding queues)
    int numThreads;

    vector<LockFreeDeque<Job> *> &getAllQueues();
};
