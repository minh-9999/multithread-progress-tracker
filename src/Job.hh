#pragma once
#include <functional>

struct Job
{
    std::function<void()> tasks;
    int priority = 0;
    int retryCount = 0;
    int timeoutMs = 0;

    void execute() const
    {
        if (tasks)
            tasks();
    }
};
