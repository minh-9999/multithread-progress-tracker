#pragma once
#include "LockFreeDeque.hh"
#include "Job.hh"

#include <atomic>
#include <thread>
#include <vector>

class Worker
{
public:
    Worker(LockFreeDeque<Job> &localQueue, vector<LockFreeDeque<Job> *> &all);
    void start();
    void stop();
    void join();

private:
    LockFreeDeque<Job> &queues;
    vector<LockFreeDeque<Job> *> &allQueues;
    thread threads;
    atomic<bool> running{true};

    void run();
    bool steal(unique_ptr<Job> &job);
};
