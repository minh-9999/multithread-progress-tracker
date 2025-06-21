#include <gtest/gtest.h>

#include "../src/Worker.hh"
#include "../src/LockFreeDeque.hh"
#include "../src/Job.hh"

using namespace std;

TEST(WorkerTest, SingleJobExecuted)
{
    LockFreeDeque<Job> localQueue;
    vector<LockFreeDeque<Job> *> allQueues = {&localQueue};

    atomic<bool> jobExecuted = false;
    atomic<bool> onResultCalled = false;

    Job job;
    job.id = "test-job";
    job.tasks = [&]()
    {
        this_thread::sleep_for(chrono::milliseconds(50));
        jobExecuted = true;
    };
    job.onResult = [&](const JobResult &)
    {
        onResultCalled = true;
    };

    auto pjob = std::make_unique<Job>();
    *pjob = std::move(job);
    localQueue.pushBottom(std::move(pjob));

    Worker worker(localQueue, allQueues);

    // Wait up to 1 second for the job to be executed
    for (int i = 0; i < 100 && !jobExecuted; ++i)
        this_thread::sleep_for(chrono::milliseconds(10));

    worker.stop();
    worker.join();

    EXPECT_TRUE(jobExecuted);
    EXPECT_TRUE(onResultCalled);
}

TEST(WorkerTest, JobStealWorks)
{
    LockFreeDeque<Job> queue1;
    LockFreeDeque<Job> queue2;
    vector<LockFreeDeque<Job> *> allQueues = {&queue1, &queue2};

    atomic<int> count{0};

    // Push 5 jobs to queue1
    for (int i = 0; i < 5; ++i)
    {
        Job job;
        job.id = "job" + to_string(i);
        job.tasks = [&count]()
        {
            this_thread::sleep_for(chrono::milliseconds(10));
            count.fetch_add(1);
        };
        queue1.pushBottom(make_unique<Job>(std::move(job)));
    }

    Worker worker1(queue1, allQueues);
    Worker worker2(queue2, allQueues); // queue2 is empty but can steal from queue1

    // Wait for jobs to be executed
    for (int i = 0; i < 100 && count.load() < 5; ++i)
    {
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    worker1.stop();
    worker2.stop();

    worker1.join();
    worker2.join();

    EXPECT_EQ(count.load(), 5);
}

TEST(WorkerTest, SingleJobExecutedWithAllCallbacks)
{
    LockFreeDeque<Job> localQueue;
    vector<LockFreeDeque<Job> *> allQueues = {&localQueue};

    atomic<bool> jobExecuted = false;
    atomic<bool> onResultCalled = false;
    atomic<bool> onCompleteCalled = false;
    atomic<bool> onAttemptCalled = false;
    atomic<bool> onStartCalled = false;

    Job job;
    job.id = "test-job";
    job.retryCount = 0; // No retry needed
    job.tasks = [&]()
    {
        this_thread::sleep_for(chrono::milliseconds(50));
        jobExecuted = true;
    };

    job.onStart = [&]()
    {
        onStartCalled = true;
    };

    job.onResult = [&](const JobResult &)
    {
        onResultCalled = true;
    };

    job.onComplete = [&](bool success, int attempt, long long durationMs)
    {
        onCompleteCalled = true;
        EXPECT_TRUE(success);
        EXPECT_EQ(attempt, 1);
        EXPECT_GE(durationMs, 50);
    };

    job.onAttempt = [&](int attempt, bool success, long long elapsed, string_view errorMsg)
    {
        onAttemptCalled = true;
        EXPECT_TRUE(success);
        EXPECT_EQ(attempt, 1);
        EXPECT_TRUE(errorMsg.empty());
    };

    localQueue.pushBottom(make_unique<Job>(std::move(job)));

    Worker worker(localQueue, allQueues);

    for (int i = 0; i < 100 && !jobExecuted; ++i)
        this_thread::sleep_for(chrono::milliseconds(10));

    worker.stop();
    worker.join();

    EXPECT_TRUE(jobExecuted);
    EXPECT_TRUE(onStartCalled);
    EXPECT_TRUE(onResultCalled);
    EXPECT_TRUE(onAttemptCalled);
    EXPECT_TRUE(onCompleteCalled);
}

TEST(WorkerTest, JobStealWithCallbacks)
{
    LockFreeDeque<Job> queue1;
    LockFreeDeque<Job> queue2;
    vector<LockFreeDeque<Job> *> allQueues = {&queue1, &queue2};

    atomic<int> executedCount{0};
    atomic<int> onCompleteCount{0};
    atomic<int> onAttemptCount{0};
    atomic<int> onResultCount{0};

    for (int i = 0; i < 5; ++i)
    {
        Job job;
        job.id = "job" + to_string(i);
        job.tasks = [&]()
        {
            this_thread::sleep_for(chrono::milliseconds(20));
            executedCount.fetch_add(1);
        };

        job.onResult = [&](const JobResult &)
        {
            onResultCount.fetch_add(1);
        };

        job.onAttempt = [&](int attempt, bool success, long long elapsed, string_view errorMsg)
        {
            onAttemptCount.fetch_add(1);
            EXPECT_TRUE(success);
        };

        job.onComplete = [&](bool success, int attempt, long long elapsed)
        {
            onCompleteCount.fetch_add(1);
        };

        queue1.pushBottom(make_unique<Job>(std::move(job)));
    }

    Worker worker1(queue1, allQueues);
    Worker worker2(queue2, allQueues); // Steals from queue1

    // Wait until all jobs are processed
    for (int i = 0; i < 200 && executedCount.load() < 5; ++i)
    {
        this_thread::sleep_for(chrono::milliseconds(10));
    }

    worker1.stop();
    worker2.stop();
    worker1.join();
    worker2.join();

    EXPECT_EQ(executedCount.load(), 5);
    EXPECT_EQ(onResultCount.load(), 5);
    EXPECT_EQ(onAttemptCount.load(), 5);
    EXPECT_EQ(onCompleteCount.load(), 5);
}

TEST(WorkerTest, JobErrorTriggersOnError)
{
    LockFreeDeque<Job> queue;
    vector<LockFreeDeque<Job> *> allQueues = {&queue};

    atomic<bool> onErrorCalled = false;
    atomic<bool> onResultCalled = false;
    atomic<bool> onCompleteCalled = false;

    Job job;
    job.id = "error-job";
    job.tasks = []
    {
        throw std::runtime_error("intentional failure");
    };
    job.retryCount = 0;

    job.onError = [&](const string &err)
    {
        onErrorCalled = true;
        EXPECT_NE(err.find("intentional failure"), string::npos);
    };

    job.onResult = [&](const JobResult &result)
    {
        onResultCalled = true;
        EXPECT_FALSE(result.success);
        EXPECT_EQ(result.attempts, 1);
    };

    job.onComplete = [&](bool success, int attempt, long long elapsed)
    {
        onCompleteCalled = true;
        EXPECT_FALSE(success);
        EXPECT_EQ(attempt, 1);
    };

    queue.pushBottom(make_unique<Job>(std::move(job)));
    Worker worker(queue, allQueues);

    for (int i = 0; i < 50 && !onCompleteCalled; ++i)
        this_thread::sleep_for(chrono::milliseconds(10));

    worker.stop();
    worker.join();

    EXPECT_TRUE(onErrorCalled);
    EXPECT_TRUE(onResultCalled);
    EXPECT_TRUE(onCompleteCalled);
}

TEST(WorkerTest, JobTimeoutTriggersOnTimeout)
{
    LockFreeDeque<Job> queue;
    vector<LockFreeDeque<Job> *> allQueues = {&queue};

    atomic<bool> onTimeoutCalled = false;
    atomic<bool> onCompleteCalled = false;
    atomic<bool> onResultCalled = false;

    Job job;
    job.id = "timeout-job";
    job.timeoutMs = 50;
    job.tasks = []
    {
        this_thread::sleep_for(chrono::milliseconds(200)); // quá timeout
    };

    job.onTimeout = [&]()
    {
        onTimeoutCalled = true;
    };

    job.onResult = [&](const JobResult &result)
    {
        onResultCalled = true;
        EXPECT_FALSE(result.success);
    };

    job.onComplete = [&](bool success, int attempt, long long elapsed)
    {
        onCompleteCalled = true;
        EXPECT_FALSE(success);
        EXPECT_GE(elapsed, 50);
    };

    queue.pushBottom(make_unique<Job>(std::move(job)));
    Worker worker(queue, allQueues);

    for (int i = 0; i < 50 && !onCompleteCalled; ++i)
        this_thread::sleep_for(chrono::milliseconds(10));

    worker.stop();
    worker.join();

    EXPECT_TRUE(onTimeoutCalled);
    EXPECT_TRUE(onResultCalled);
    EXPECT_TRUE(onCompleteCalled);
}

TEST(WorkerTest, JobRetriesOnFailure)
{
    LockFreeDeque<Job> queue;
    vector<LockFreeDeque<Job> *> allQueues = {&queue};

    atomic<int> failCount = 0;
    atomic<int> attemptCount = 0;
    atomic<bool> onResultCalled = false;

    Job job;
    job.id = "retry-job";
    job.retryCount = 3;

    job.tasks = [&]()
    {
        if (failCount.fetch_add(1) < 2)
            throw std::runtime_error("fail");
        // succeed on third attempt
    };

    job.onAttempt = [&](int attempt, bool success, long long elapsed, string_view errorMsg)
    {
        attemptCount.fetch_add(1);
        if (attempt < 3)
            EXPECT_FALSE(success);
        else
            EXPECT_TRUE(success);
    };

    job.onResult = [&](const JobResult &result)
    {
        onResultCalled = true;
        EXPECT_TRUE(result.success);
        EXPECT_EQ(result.attempts, 3);
    };

    queue.pushBottom(make_unique<Job>(std::move(job)));
    Worker worker(queue, allQueues);

    for (int i = 0; i < 100 && !onResultCalled; ++i)
        this_thread::sleep_for(chrono::milliseconds(10));

    worker.stop();
    worker.join();

    EXPECT_EQ(failCount.load(), 3);
    EXPECT_EQ(attemptCount.load(), 3);
    EXPECT_TRUE(onResultCalled);
}
