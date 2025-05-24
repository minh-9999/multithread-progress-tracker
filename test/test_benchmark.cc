#include <gtest/gtest.h>
// #include "../src/JobDispatcher.hh"
#include "../src/benchmark.hh"

#include <iostream>

using namespace std;

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