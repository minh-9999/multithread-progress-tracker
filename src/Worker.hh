#pragma once
#include "LockFreeDeque.hh"
#include "Job.hh"
#include <atomic>
#include <thread>
#include <vector>

class Worker
{
public:
    Worker(LockFreeDeque<Job> &localQueue, std::vector<LockFreeDeque<Job> *> &all);
    void start();
    void stop();
    void join();

private:
    LockFreeDeque<Job> &queue;
    std::vector<LockFreeDeque<Job> *> &allQueues;
    std::thread thread;
    std::atomic<bool> running{true};

    void run();
    bool steal(Job &job);
};
