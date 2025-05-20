#include <gtest/gtest.h>
#include "../src/JobDispatcher.hh"
#include "../src/Logger.hh"

// Helper để kiểm tra nội dung file log
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

    atomic<bool> jobExecuted = false;

    JobDispatcher dispatcher(1);
    Job job;
    job.tasks = [&]()
    {
        Logger::dualSafeLog("Job executed.");
        jobExecuted = true;
    };

    dispatcher.dispatch(0, job);
    this_thread::sleep_for(chrono::milliseconds(100));
    dispatcher.stop();

    EXPECT_TRUE(jobExecuted.load());

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
        dispatcher.dispatch(i % 2, Job{[&counter]()
                                       {
                                           counter.fetch_add(1);
                                       }});
    }

    this_thread::sleep_for(chrono::milliseconds(300));
    dispatcher.stop();

    EXPECT_EQ(counter.load(), jobCount);
}

TEST(JobDispatcherTest, JobStealingWorks)
{
    Logger::init("steal_log.txt");

    atomic<int> thread1Executed = 0;

    // Use 2 threads, only push job to thread 0
    JobDispatcher dispatcher(2);

    // Create more jobs than thread #0 can process at once
    for (int i = 0; i < 10; ++i)
    {
        Job job;
        job.tasks = [&thread1Executed]()
        {
            this_thread::sleep_for(chrono::milliseconds(10));
            thread1Executed++;
        };
        dispatcher.dispatch(0, job); // always push to thread 0
    }

    this_thread::sleep_for(chrono::milliseconds(500));
    dispatcher.stop();

    // If thread #1 has successfully stolen, jobExecuted > 0
    EXPECT_GE(thread1Executed.load(), 1);
}
