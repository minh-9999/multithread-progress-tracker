#pragma once

#include "Job.hh"
#include "Logger.hh"

#include <thread>
#include <future>

class JobExecutor
{
public:
    static JobResult execute(const Job &job)
    {
        JobResult result;
        result.jobId = job.id;
        result.category = job.category;
        result.startTime = system_clock::now();

        bool success = false;
        string errorMessage;
        int attempts = 0;
        long long durationMs = 0;
        long long lastAttemptDuration = 0;

        if (job.onStart)
            job.onStart();

        auto overallStart = steady_clock::now(); // measure total job time

        for (; attempts <= job.retryCount; ++attempts)
        {
            auto attemptStart = steady_clock::now();

            success = tryRun(job, errorMessage, job.timeoutMs);

            auto attemptEnd = steady_clock::now();
            lastAttemptDuration = duration_cast<milliseconds>(attemptEnd - attemptStart).count();

            if (success)
                break;

            if (!success && !errorMessage.empty() && job.onError)
                job.onError(errorMessage);
        }

        auto overallEnd = steady_clock::now();
        long long totalDurationMs = duration_cast<milliseconds>(overallEnd - overallStart).count();

        result.success = success;
        result.attempts = attempts;
        result.durationMs = totalDurationMs;
        result.endTime = system_clock::now();

        if (!success && !errorMessage.empty())
            result.errorMessage = errorMessage;

        if (!success && job.onTimeout && totalDurationMs >= job.timeoutMs)
            job.onTimeout();

        // If all fails & timeout callback is set
        if (!success && durationMs >= job.timeoutMs && job.onTimeout)
            job.onTimeout();

        // Log & Callback
        Logger::dualSafeLog("[JobExecutor] Job " + job.id + " done in " + to_string(durationMs) + "ms. " + (success ? "Success" : "Failed"));

        if (job.onResult)
            job.onResult(result);

        return result;
    }

private:
    static bool tryRun(const Job &job, string &error, int timeoutMs)
    {
        try
        {
            if (timeoutMs > 0)
            {
                // Run with timeout
                packaged_task<void()> task(job.tasks);
                auto fut = task.get_future();
                thread t(std::move(task));

                if (fut.wait_for(chrono::milliseconds(timeoutMs)) == future_status::timeout)
                {
                    error = "Timeout after " + to_string(timeoutMs) + "ms";
                    t.detach(); // We abandon the thread — risky in prod, okay for demo
                    return false;
                }

                t.join();
            }
            else
            {
                // No timeout
                job.tasks();
            }
            return true;
        }

        catch (const exception &e)
        {
            error = e.what();
            return false;
        }

        catch (...)
        {
            error = "Unknown exception";
            return false;
        }
    }
};
