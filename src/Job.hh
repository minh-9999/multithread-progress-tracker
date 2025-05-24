#pragma once
#include <iostream>
#include <functional>
#include <chrono>
#include <exception>

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

    // Optional callback when job completes or fails
    function<void(bool success, int attempt, long long durationMs)> onComplete =
        nullptr;

    string category = "default"; // to track by group in ProgressTracker
                                 //
    // Callback after each retry (whether successful or failed)
    function<void(int attempt, bool success, long long elapsed, string_view errorMsg)> onAttempt = nullptr;

    Job() = default;

    template <typename Callable>
    Job(Callable &&fn) : tasks(std::forward<Callable>(fn)) {}

    void execute()
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
                // success = false;
            }
            catch (...)
            {
                errorMsg = "Unknown exception";
                // success = false;
            }

            auto duration = chrono::steady_clock::now() - start;
            long long elapsed = chrono::duration_cast<chrono::milliseconds>(duration).count();

            // Log each run
            cout << "[Job] Attempt " << attempt + 1
                 << ", Time: " << elapsed << " ms"
                 << (success ? ", Status: OK" : ", Status: FAILED") << "\n";

            // if (!success)
            // {
            //     cerr << "[Job] Exception: " << errorMsg << "\n";
            // }

            if (onAttempt)
                onAttempt(attempt + 1, success, elapsed, errorMsg);

            if (success && elapsed <= timeoutMs)
            {
                status = JobStatus::Success;

                if (onComplete)
                    onComplete(true, attempt + 1, elapsed);

                break; // Task was successful and did not time out
            }

            if (attempt == retryCount)
            {
                status = (success ? JobStatus::Success
                                  : (elapsed > timeoutMs ? JobStatus::Timeout
                                                         : JobStatus::Failed));

                if (onComplete)
                    onComplete(false, attempt + 1, elapsed);
            }
        }
    }
};