#pragma once

#include <chrono>
#include <functional>
#include <vector>
#include <atomic>

#include "Logger.hh"

// #include <nlohmann/json.hpp> // JSON export
#include "../third_party/json-src/single_include/nlohmann/json.hpp" // JSON export

using json = nlohmann::json;

// Save simple statistics of a job group
struct JobMetric
{
    atomic<int> count{0};
    vector<int> latencies; // Latency History

    // int minLatency = INT_MAX;
    // int maxLatency = INT_MIN;
};

// Manage statistics by group
struct CategoryMetric
{
    vector<int> latencies;                 // Latency History
    unordered_map<LogLevel, int> lvlCount; // count by LogLevel
    atomic<int> count{0};                  // Count total logs
    mutable mutex mtx_;
    int minLatency = INT_MAX;
    int maxLatency = INT_MIN;

    // handle latency and min/max updates in a thread-safe manner
    void addLatency(int latencyMs)
    {
        lock_guard<mutex> lock(mtx_);
        latencies.push_back(latencyMs);

        if (latencyMs < minLatency)
            minLatency = latencyMs;

        if (latencyMs > maxLatency)
            maxLatency = latencyMs;
    }
};

/*
✅ Track the number of completed jobs
✅ Calculate latency statistics (min/max/average)
✅ Print progress periodically
✅ Support safe multi-threading
✅ Export data to JSON or Prometheus
✅ Can be used as an auxiliary tool for batch, pipeline, or background job processing systems.
*/
class ProgressTracker
{
public:
    using Callback = function<void(int jobIndex, int latency, const string &category)>;

    explicit ProgressTracker(int totalJobs);

    // status updates, latency statistics and log levels
    void markJobDone(int latencyMs, LogLevel level);
    void markJobDoneWithCategory(const string &category, int latencyMs, LogLevel level);
    void updateProgress();
    void finish();

    // Optional configs
    void pause();
    void resume();
    void setLogInterval(int jobCount);
    void setHighlightLatency(int thresholdMs);
    void setEnableColor(bool enable);
    // string exportSummaryJSON() const;
    // string exportSummaryJSON();
    json exportSummaryJSON();

    string exportPrometheus() const;
    string exportJSON() const;

    // Create a function to expose HTTP server metrics, run async (separate thread)
    void startHTTPServer(int port = 8080);

    void printLevelSummary();

    json exportLevelSummaryJSON();

private:
    // Total number of jobs to be processed
    int total;
    // Number of completed jobs
    atomic<int> done{0};
    // vector<int> latencies;
    // total latency (ms) of all completed jobs
    atomic<int> latencySum{0};
    // The number of jobs included in latencySum
    atomic<int> latencyCount{0};

    mutable mutex latencyMutex;
    chrono::steady_clock::time_point startTime;
    atomic<bool> isPaused{false};

    // How many jobs to log at once?
    int logInterval = 1;
    // Last logged in job number
    int lastLoggedDone = 0;
    // If latency > threshold then mark special
    int highlightThreshold = -1;
    bool enableColor = false;

    string formatETA() const;

    int averageLatency() const;

    string colorText(const string &text, const string &colorCode) const;

    // Statistics by job type
    unordered_map<string, CategoryMetric> categoryMetrics;
    Callback callback;
    atomic<int> totalDone{0}; // total number of actual jobs processed

    const vector<int> latencyBuckets = {50, 100, 250, 500, 1000};

    // Number of logs by category and log level
    unordered_map<string, unordered_map<LogLevel, int>> categoryLevelCounts;
    mutex levelCountMutex;
};
