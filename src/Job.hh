#pragma once

#include <iostream>
#include <functional>
#include <chrono>
#include <exception>

#include "JobResult.hh"

using namespace std;

enum class JobStatus
{
    Pending,
    Running,
    Success,
    Failed,
    Timeout
};

struct Job
{
    string id;
    function<void()> tasks;
    int priority = 0;
    int retryCount = 0;
    int timeoutMs = 0;

    atomic<JobStatus> status{JobStatus::Pending};

    // Callback when job ends, success or failure, number of attempts, running time (ms)
    function<void(bool success, int attempt, long long durationMs)> onComplete =
        nullptr;

    string category = "default"; // to track by group in ProgressTracker
                                 //
    // Callback after each retry (whether successful or failed)
    function<void(int attempt, bool success, long long elapsed, string_view errorMsg)> onAttempt = nullptr;

    // Callback when job completes
    function<void(const JobResult &)> onResult = nullptr;

    function<void()> onStart;
    function<void(const string &error)> onError;
    function<void()> onTimeout;

    Job() = default;

    // Move ctor
    Job(Job &&other) noexcept
        : id(std::move(other.id)),
          tasks(std::move(other.tasks)),
          priority(other.priority),
          retryCount(other.retryCount),
          timeoutMs(other.timeoutMs),
          status(other.status.load()), // load atomic value
          onComplete(std::move(other.onComplete)),
          category(std::move(other.category)),
          onAttempt(std::move(other.onAttempt)),
          onResult(std::move(other.onResult)),
          onStart(std::move(other.onStart)),
          onError(std::move(other.onError)),
          onTimeout(std::move(other.onTimeout))
    {
        other.status = JobStatus::Pending; // reset the power side state if necessary
    }

    // Move assignment operator
    Job &operator=(Job &&other) noexcept
    {
        if (this != &other)
        {
            id = std::move(other.id);
            tasks = std::move(other.tasks);
            priority = other.priority;
            retryCount = other.retryCount;
            timeoutMs = other.timeoutMs;
            status.store(other.status.load());
            onComplete = std::move(other.onComplete);
            category = std::move(other.category);
            onAttempt = std::move(other.onAttempt);
            onResult = std::move(other.onResult);
            onStart = std::move(other.onStart);
            onError = std::move(other.onError);
            onTimeout = std::move(other.onTimeout);

            other.status = JobStatus::Pending;
        }
        return *this;
    }

    // Delete copy ctor + copy assignment
    Job(const Job &) = delete;
    Job &operator=(const Job &) = delete;

    // template <typename Callable>
    // Job(Callable &&fn) : tasks(std::forward<Callable>(fn)) {}

    template <typename Callable, typename = enable_if_t<!is_same<decay_t<Callable>, Job>::value>>
    Job(Callable &&fn) : tasks(std::forward<Callable>(fn)) {}

    bool execute()
    {
        status = JobStatus::Running;

        for (int attempt = 0; attempt <= retryCount; ++attempt)
        {
            auto start = chrono::steady_clock::now();
            bool success = false;
            string errorMsg;

            try
            {
                tasks(); // Can throw exception
                success = true;
            }
            catch (const exception &e)
            {
                errorMsg = e.what();

                if (onError)
                    onError(errorMsg);
            }
            catch (...)
            {
                errorMsg = "Unknown exception";

                if (onError)
                    onError(errorMsg);
            }

            auto duration = chrono::steady_clock::now() - start;
            long long elapsed = chrono::duration_cast<chrono::milliseconds>(duration).count();

            // Log each run
            cout << "[Job] Attempt " << attempt
                 << ", Time: " << elapsed << " ms"
                 << (success ? ", Status: OK" : ", Status: FAILED") << "\n";

            if (onAttempt)
                onAttempt(attempt, success, elapsed, errorMsg);

            if (success && (timeoutMs == 0 || elapsed <= timeoutMs))
            {
                status = JobStatus::Success;
                if (onComplete)
                    onComplete(true, attempt, elapsed);
                return true;
            }

            if (timeoutMs > 0 && elapsed > timeoutMs)
            {
                status = JobStatus::Timeout;
                if (onTimeout)
                    onTimeout();
                if (onComplete)
                    onComplete(false, attempt, elapsed);
                return false;
            }
        }

        status = JobStatus::Failed;
        if (onComplete)
            onComplete(false, retryCount + 1, 0);

        return false;
    }
};