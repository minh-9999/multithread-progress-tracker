#pragma once
#include <functional>
#include <chrono>

using namespace std;

struct Job
{
    function<void()> tasks;
    int priority = 0;
    int retryCount = 0;
    int timeoutMs = 0;

    void execute() const
    {
        for (int i = 0; i <= retryCount; ++i)
        {
            auto start = chrono::steady_clock::now();
            tasks();
            auto duration = chrono::steady_clock::now() - start;
            if (chrono::duration_cast<chrono::milliseconds>(duration).count() <= timeoutMs)
                break;
        }
    }
};
