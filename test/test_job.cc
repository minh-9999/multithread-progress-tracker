#include <gtest/gtest.h>
#include "../src/Job.hh"
#include "../src/Logger.hh"

TEST(JobTest, RetryAndTimeoutSimulation)
{
    int maxRetries = 3;
    int attempt = 0;
    bool completed = false;

    Job job;
    job.retryCount = maxRetries;
    job.timeoutMs = 100; // giả lập timeout dùng sleep

    job.tasks = [&]()
    {
        attempt++;

        if (attempt <= 2)
        {
            this_thread::sleep_for(chrono::milliseconds(150)); // timeout
            Logger::dualSafeLog("Simulate failure on attempt " + to_string(attempt));
        }
        else
        {
            completed = true;
            Logger::dualSafeLog("Job succeeded on attempt " + to_string(attempt));
        }
    };

    for (int i = 0; i <= job.retryCount && !completed; ++i)
    {
        auto start = chrono::steady_clock::now();
        job.execute();
        auto end = chrono::steady_clock::now();
        int duration = chrono::duration_cast<chrono::milliseconds>(end - start).count();

        if (duration > job.timeoutMs)
        {
            Logger::dualSafeLog("Timeout on attempt " + to_string(i + 1));
        }
    }

    EXPECT_TRUE(completed);
    EXPECT_EQ(attempt, 3);
}
