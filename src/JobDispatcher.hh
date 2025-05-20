#pragma once

#include "LockFreeDeque.hh"
#include "Worker.hh"
#include "Job.hh"

class JobDispatcher
{
public:
    explicit JobDispatcher(int n);
    void dispatch(int threadIndex, Job job);
    void stop();

private:
    std::vector<std::unique_ptr<LockFreeDeque<Job>>> queues;
    std::vector<std::unique_ptr<Worker>> workers;
    int numThreads;

    std::vector<LockFreeDeque<Job> *> &getAllQueues();
};
