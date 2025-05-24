#pragma once

#include "LockFreeDeque.hh"
#include "Worker.hh"
#include "Job.hh"

using namespace std;

class JobDispatcher
{
public:
    explicit JobDispatcher(int n);
    void dispatch(int threadIndex, unique_ptr<Job> job);
    void stop();

private:
    vector<unique_ptr<LockFreeDeque<Job>>> queues;
    vector<unique_ptr<Worker>> workers;
    int numThreads;

    vector<LockFreeDeque<Job> *> &getAllQueues();
};
