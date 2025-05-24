#pragma once
#include "Job.hh"
#include "Logger.hh"

using namespace std;
using namespace chrono;

class JobExecutor
{
public:
    static void run(unique_ptr<Job> job)
    {

        for (int attempt = 0; attempt <= job->retryCount; ++attempt)
        {
            auto start = steady_clock::now();
            bool success = false;
            string errorMsg;

            try
            {
                job->tasks();
                success = true;
            }
            catch (const exception &e)
            {
                errorMsg = e.what();
                success = false;
            }
            catch (...)
            {
                errorMsg = "Unknown exception";
                success = false;
            }

            auto duration = steady_clock::now() - start;
            long long elapsed = duration_cast<milliseconds>(duration).count();

            Logger::dualSafeLog("[JobExecutor] Attempt " + to_string(attempt + 1) +
                                ", Time: " + to_string(elapsed) + " ms" +
                                (success ? ", Status: OK" : ", Status: FAILED"));

            if (!success)
            {
                Logger::dualSafeLog("[JobExecutor] Exception: " + errorMsg);
            }

            if (success && elapsed <= job->timeoutMs)
            {
                if (job->onComplete)
                    job->onComplete(true, attempt + 1, elapsed);
                break;
            }

            if (attempt == job->retryCount)
            {
                if (job->onComplete)
                    job->onComplete(false, attempt + 1, elapsed);
            }
        }
    }
};
