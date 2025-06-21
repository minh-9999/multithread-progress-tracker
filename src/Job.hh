#pragma once

#include <iostream>
#include <functional>
#include <exception>
#include <type_traits>

#include "JobResult.hh"
#include "log_utils.h"

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
    // Unique identifier for the job
    string id;
    // Main execution function of the job
    function<void()> tasks;
    // Priority of the job (the larger the value, the higher the priority)
    int priority = 0;
    // Number of retries allowed if job fails
    int retryCount = 0;
    // Maximum time (in milliseconds) for a job run
    int timeoutMs = 0;

    // Current status of the job (Pending, Running, Succeeded, Failed, etc.)
    atomic<JobStatus> status{JobStatus::Pending};

    // Callback is called when the job finishes (whether successful or failed)
    // - success: indicates whether the job was successful or not
    // - attempt: number of attempts to execute
    // - durationMs: running time (in milliseconds)
    function<void(bool success, int attempt, long long durationMs)> onComplete = nullptr;

    // Group/category of work, used for sorting or tracking (e.g. via ProgressTracker)
    string category = "default";

    // Callback is called after each attempt to execute the job
    // - attempt: which attempt
    // - success: whether the job was successful in this attempt
    // - elapsed: execution time in the attempt (milliseconds)
    // - errorMsg: error message if any
    function<void(int attempt, bool success, long long elapsed, string_view errorMsg)> onAttempt = nullptr;

    // Callback is called when the job completes (success or failure)
    // - JobResult contains detailed result information
    function<void(const JobResult &)> onResult = nullptr;

    // Callback is called as soon as the job starts running
    function<void()> onStart;

    // Callback is called if the job encounters an error during execution
    // - error: error information as a string
    function<void(const string &error)> onError;

    // Callback is called if the job times out
    function<void()> onTimeout;

    Job() = default;

    // Move constructor: move resources from another Job (no copy)
    Job(Job &&other) noexcept : id(std::move(other.id)),
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

    // Move assignment operator: transfers ownership of resources from another Job instance
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

    /*
    Constructor template that accepts a callable (e.g., lambda or function object) as a single task.
    This constructor is disabled if the argument is of type Job, to avoid ambiguity with copy/move constructors.
    */
    // template <typename Callable, typename = enable_if_t<!is_same<decay_t<Callable>, Job>::value>>
    // Job(Callable &&fn) : tasks(std::forward<Callable>(fn)) {}

    // Create aliases for readability and reusability
    template <typename H>
    using is_not_job = enable_if_t<!is_same_v<decay_t<H>, Job>>;

    template <typename Callable, typename = is_not_job<Callable>>
    Job(Callable &&fn) : tasks(std::forward<Callable>(fn)) {}

    /*
    for C++20
    */
    // template <typename Callable>
    //     requires(!is_same_v<decay_t<Callable>, Job>)
    // Job(Callable &&fn) : tasks(std::forward<Callable>(fn))
    // {
    // }

    /*
    runs a job (defined in tasks()) with the ability to retry multiple times (retryCount) if it fails.
    It manages the state of the job, measures its running time, handles exceptions, and
    calls callbacks to report progress, errors, timeouts, or final results.
    */
    bool execute()
    {
        // Mark the running job status
        status = JobStatus::Running;

        // Try doing the job multiple times if necessary.
        for (int attempt = 0; attempt <= retryCount; ++attempt)
        {
            auto start = chrono::steady_clock::now();
            bool success = false; // mark run results
            string errorMsg;

            try
            {
                tasks();        // Can throw exception
                success = true; // If no error, mark success
            }
            catch (const exception &e)
            {
                // Catch standard exception, get error message
                errorMsg = e.what();

                // Call callback to handle error if any
                if (onError)
                    onError(errorMsg);
            }
            catch (...)
            {
                // Catch unknown exception, write general message
                errorMsg = "Unknown exception";

                // Call callback to handle error if any
                if (onError)
                    onError(errorMsg);
            }

            auto duration = chrono::steady_clock::now() - start;
            long long elapsed = chrono::duration_cast<chrono::milliseconds>(duration).count();

            // Log the test results
            {
                lock_guard<mutex> lock(g_logMutex);
                cout << "[Job] Attempt " << attempt
                     << ", Time: " << elapsed << " ms"
                     << (success ? ", Status: OK" : ", Status: FAILED") << "\n";
            }

            // Call callback to report test result if any
            if (onAttempt)
                onAttempt(attempt, success, elapsed, errorMsg);

            if (success && (timeoutMs == 0 || elapsed <= timeoutMs))
            {
                // Update status successfully
                status = JobStatus::Success;

                if (onComplete)
                    onComplete(true, attempt, elapsed); // Call callback on successful completion

                return true;
            }

            // If there is a timeout and the running time exceeds the limit
            if (timeoutMs > 0 && elapsed > timeoutMs)
            {
                // Update timeout status
                status = JobStatus::Timeout;

                if (onTimeout)
                    onTimeout(); // Call the timeout handling callback

                if (onComplete)
                    onComplete(false, attempt, elapsed); // Calling the completion callback failed due to timeout

                return false;
            }
        }

        // If all retryCount attempts fail
        status = JobStatus::Failed; // Update failed status
        if (onComplete)
            onComplete(false, retryCount + 1, 0); // Call callback on failed completion

        return false;
    }
};