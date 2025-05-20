#include <gtest/gtest.h>
#include "../src/JobDispatcher.hh"

void runBenchmark(int numThreads, int numJobs, int sleepPerJobMs, int &durationMs)
{
    JobDispatcher dispatcher(numThreads);
    atomic<int> done{0};

    for (int i = 0; i < numJobs; ++i)
    {
        dispatcher.dispatch(i % numThreads, Job{[&]()
                                                {
                                                    this_thread::sleep_for(chrono::milliseconds(sleepPerJobMs));
                                                    done++;
                                                }});
    }

    auto start = chrono::steady_clock::now();
    while (done.load() < numJobs)
    {
        this_thread::sleep_for(chrono::milliseconds(10));
    }
    auto end = chrono::steady_clock::now();
    dispatcher.stop();

    durationMs = chrono::duration_cast<chrono::milliseconds>(end - start).count();
}

TEST(JobDispatcherBenchmark, CompareThreads)
{
    int jobCount = 50;
    int sleepMs = 20;

    int durationSingle = 0;
    int durationMulti = 0;

    runBenchmark(1, jobCount, sleepMs, durationSingle);
    runBenchmark(4, jobCount, sleepMs, durationMulti);

    cout << "[1 Thread] Total time: " << durationSingle << "ms\n";
    cout << "[4 Threads] Total time: " << durationMulti << "ms\n";

    EXPECT_GT(durationSingle, durationMulti * 0.9); // No need to be too strict
}