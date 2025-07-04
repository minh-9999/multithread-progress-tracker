#include <gtest/gtest.h>
#include "../src/ProgressTracker.hh"

TEST(ProgressTrackerTest, MarkJobDoneAndFinish)
{
    const int totalJobs = 5;
    ProgressTracker tracker(totalJobs);

    tracker.setEnableColor(false);
    tracker.setHighlightLatency(100);
    tracker.setLogInterval(1);

    for (int i = 0; i < totalJobs; ++i)
    {
        tracker.markJobDone(50 + i * 10, LogLevel::Info); // latency gradually increases
    }

    tracker.finish();
    string json = tracker.exportSummaryJSON();

    EXPECT_NE(json.find("\"total_jobs\": 6"), string::npos);
    EXPECT_NE(json.find("\"completed_jobs\": 6"), string::npos);
    EXPECT_NE(json.find("\"average_latency_ms\""), string::npos);
    EXPECT_NE(json.find("\"warn\""), string::npos);
}

TEST(ProgressTrackerTest, MarkJobDoneWithCategory)
{
    ProgressTracker tracker(6);

    tracker.setEnableColor(false);
    tracker.setHighlightLatency(100);
    tracker.setLogInterval(2);

    // Simulate 3 job groups
    tracker.markJobDoneWithCategory("IO", 50, LogLevel::Info);
    tracker.markJobDoneWithCategory("IO", 70, LogLevel::Info);
    tracker.markJobDoneWithCategory("CPU", 150, LogLevel::Warn);
    tracker.markJobDoneWithCategory("CPU", 200, LogLevel::Error);
    tracker.markJobDoneWithCategory("NET", 90, LogLevel::Info);
    tracker.markJobDoneWithCategory("NET", 120, LogLevel::Warn);

    string json = tracker.exportJSON();

    EXPECT_NE(json.find("\"IO\""), string::npos);
    EXPECT_NE(json.find("\"CPU\""), string::npos);
    EXPECT_NE(json.find("\"NET\""), string::npos);
    EXPECT_NE(json.find("\"average_latency_ms\""), string::npos);

    // If you want to check the exact number of jobs per group:
    EXPECT_NE(json.find("\"job_count\": 2"), string::npos);
}