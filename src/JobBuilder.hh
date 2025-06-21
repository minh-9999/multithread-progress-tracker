#pragma once

#include "Job.hh"
#include "JobResult.hh"

/*
configure and create Job objects dynamically in the "builder pattern" style
*/

class JobBuilder
{
private:
    Job job;

public:
    // main job function
    JobBuilder &withTask(function<void()> t)
    {
        job.tasks = std::move(t);
        return *this;
    }

    // Set priority for Job
    JobBuilder &withPriority(int p)
    {
        job.priority = p;
        return *this;
    }

    // Set the number of retries if the job fails
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

    // Set the ID for the Job
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

    // Set callback when job completes (success or failure)
    JobBuilder &onResult(function<void(const JobResult &)> callback)
    {
        job.onResult = std::move(callback);
        return *this;
    }

    // Set callback when job starts running
    JobBuilder &onStart(std::function<void()> callback)
    {
        job.onStart = std::move(callback);
        return *this;
    }

    // Set up a callback when the job encounters an error
    JobBuilder &onError(std::function<void(const std::string &)> callback)
    {
        job.onError = std::move(callback);
        return *this;
    }

    // Set callback when job times out
    JobBuilder &onTimeout(std::function<void()> callback)
    {
        job.onTimeout = std::move(callback);
        return *this;
    }

    // Returns the configured Job object
    Job build()
    {
        return std::move(job);
    }

    // Set callback when job completes
    JobBuilder &onComplete(function<void(bool, int, long long)> callback)
    {
        job.onComplete = std::move(callback);
        return *this;
    }

    // Set callback each time
    JobBuilder &onAttempt(function<void(int, bool, long long, string_view)> callback)
    {
        job.onAttempt = std::move(callback);
        return *this;
    }

    JobBuilder &reset()
    {
        job = Job{};
        return *this;
    }
};
