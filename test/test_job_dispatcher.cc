#include <gtest/gtest.h>
#include "../src/JobDispatcher.hh"
#include "../src/Logger.hh"

// Helper to check log file contents
string readFileContent(const string &filename)
{
    ifstream file(filename);
    ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

TEST(JobDispatcherTest, SingleJobExecution)
{
    Logger::init("test_log.txt");

    atomic<bool> jobExecuted{false};

    JobDispatcher dispatcher(1);

    auto jobPtr = make_unique<Job>([&]()
                                        {
    Logger::dualSafeLog("Job executed.");
    jobExecuted.store(true, memory_order_release); });
    dispatcher.dispatch(0, std::move(jobPtr));

    // Wait until jobExecuted is true or timeout after 1s
    auto start = chrono::steady_clock::now();
    while (!jobExecuted.load(memory_order_acquire))
    {
        this_thread::sleep_for(chrono::milliseconds(10));

        if (chrono::steady_clock::now() - start > chrono::seconds(1))
            FAIL() << "Timeout waiting for job to execute";
    }

    dispatcher.stop();

    // Flush logs if needed
    Logger::flush();

    string logContent = readFileContent("test_log.txt");
    EXPECT_NE(logContent.find("Job executed."), string::npos);
}

TEST(JobDispatcherTest, MultipleJobExecution)
{
    Logger::init("multi_log.txt");

    atomic<int> counter = 0;
    int jobCount = 10;
    JobDispatcher dispatcher(2);

    for (int i = 0; i < jobCount; ++i)
    {
        auto job = std::make_unique<Job>();
        job->tasks = [&counter]()
        {
            counter.fetch_add(1);
        };
        dispatcher.dispatch(i % 2, std::move(job));
    }

    this_thread::sleep_for(chrono::milliseconds(300));
    dispatcher.stop();

    EXPECT_EQ(counter.load(), jobCount);
}

TEST(JobDispatcherTest, JobStealingWorks)
{
    Logger::init("steal_log.txt");

    atomic<int> thread1Executed{0};

    // Use 2 threads, only push job to thread 0
    JobDispatcher dispatcher(2);

    // Create more jobs than thread #0 can process at once
    for (int i = 0; i < 10; ++i)
    {
        auto jobPtr = std::make_unique<Job>();
        jobPtr->tasks = [&thread1Executed]()
        {
            this_thread::sleep_for(chrono::milliseconds(10));
            thread1Executed++;
        };
        dispatcher.dispatch(0, std::move(jobPtr)); // push to thread 0
    }

    this_thread::sleep_for(chrono::milliseconds(500));
    dispatcher.stop();

    // If thread #1 has successfully stolen, jobExecuted > 0
    EXPECT_GE(thread1Executed.load(), 1);
}

