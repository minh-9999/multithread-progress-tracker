#pragma once

#include "Job.hh"
#include "JobResult.hh"

class JobBuilder
{
private:
    Job job;

public:
    JobBuilder &withTask(function<void()> t)
    {
        job.tasks = std::move(t);
        return *this;
    }

    JobBuilder &withPriority(int p)
    {
        job.priority = p;
        return *this;
    }

    JobBuilder &withRetry(int r)
    {
        job.retryCount = r;
        return *this;
    }

    JobBuilder &withTimeout(int t)
    {
        job.timeoutMs = t;
        return *this;
    }

    JobBuilder &withId(const string &id)
    {
        job.id = id;
        return *this;
    }

    JobBuilder &withCategory(const string &cat)
    {
        job.category = cat;
        return *this;
    }

    JobBuilder &onResult(function<void(const JobResult &)> callback)
    {
        job.onResult = std::move(callback);
        return *this;
    }

    JobBuilder &onStart(std::function<void()> callback)
    {
        job.onStart = std::move(callback);
        return *this;
    }

    JobBuilder &onError(std::function<void(const std::string &)> callback)
    {
        job.onError = std::move(callback);
        return *this;
    }

    JobBuilder &onTimeout(std::function<void()> callback)
    {
        job.onTimeout = std::move(callback);
        return *this;
    }

    Job build()
    {
        return job;
    }
};
