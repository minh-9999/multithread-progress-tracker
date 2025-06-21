
#include <gtest/gtest.h>
#include <atomic>

#include "../src/JobExecutor.hh"

TEST(JobExecutorTest, SuccessWithoutRetry)
{
    Job job;
    job.id = "job_1";
    job.tasks = []() { this_thread::sleep_for(chrono::milliseconds(50)); };
    job.timeoutMs = 200;
    job.retryCount = 0;

    JobResult result = JobExecutor::execute(job);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.attempts, 1);
    EXPECT_GE(result.durationMs, 50);
}

TEST(JobExecutorTest, TimeoutAndFail)
{
    Job job;
    job.id = "job_2";
    job.tasks = []() { this_thread::sleep_for(chrono::milliseconds(200)); };
    job.timeoutMs = 100;
    job.retryCount = 0;

    JobResult result = JobExecutor::execute(job);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.attempts, 1);
    EXPECT_GE(result.durationMs, 100);
    EXPECT_FALSE(result.errorMessage->empty());
}

TEST(JobExecutorTest, RetryUntilSuccess)
{
    atomic<int> counter{0};
    Job job;
    job.id = "job_3";
    job.retryCount = 3;
    job.timeoutMs = 0;
    job.tasks = [&]() {
        if (++counter < 3)
            throw runtime_error("fail");
    };

    JobResult result = JobExecutor::execute(job);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.attempts, 3);
    EXPECT_EQ(result.errorMessage, "");
}

TEST(JobExecutorTest, CallbackTriggered)
{
    atomic<bool> called{false};

    Job job;
    job.id = "job_4";
    job.tasks = []() {};
    job.retryCount = 0;
    job.onResult = [&](const JobResult &r) {
        called = true;
        EXPECT_EQ(r.jobId, "job_4");
    };

    JobExecutor::execute(job);
    EXPECT_TRUE(called);
}
