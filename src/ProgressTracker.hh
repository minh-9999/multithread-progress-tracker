#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <atomic>

using namespace std;

struct JobMetric
{
    atomic<int> count {0};
    vector<int> latencies;

    int minLatency = INT_MAX;
    int maxLatency = INT_MIN;
};

struct CategoryMetric
{
    vector<int> latencies;
    atomic<int> count{0};
    mutable mutex mtx_;
    int minLatency = INT_MAX;
    int maxLatency = INT_MIN;

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

class ProgressTracker
{
public:
    using Callback = function<void(int jobIndex, int latency, const string &category)>;

    explicit ProgressTracker(int totalJobs);

    void markJobDone(int latencyMs);
    void markJobDoneWithCategory(const std::string &category, int latencyMs);
    void updateProgress();
    void finish();

    // Optional configs
    void pause();
    void resume();
    void setLogInterval(int jobCount);
    void setHighlightLatency(int thresholdMs);
    void setEnableColor(bool enable);
    string exportSummaryJSON() const;

    string exportPrometheus() const;
    string exportJSON() const;

    // Create a function to expose HTTP server metrics, run async (separate thread)
    void startHTTPServer(int port = 8080);

private:
    int total;
    atomic<int> done {0};
    vector<int> latencies;
    chrono::steady_clock::time_point startTime;
    atomic<bool> isPaused{false};

    int logInterval = 1;
    int lastLoggedDone = 0;
    int highlightThreshold = -1;
    bool enableColor = false;

    string formatETA() const;
    int averageLatency() const;
    string colorText(const string &text, const string &colorCode) const;

    unordered_map<string, CategoryMetric> categoryMetrics;
    Callback callback;
    atomic<int> totalDone {0};

    const std::vector<int> latencyBuckets = {50, 100, 250, 500, 1000};
};
