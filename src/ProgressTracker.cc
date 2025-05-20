#include "../third_party/httplib/httplib.h"

#include "ProgressTracker.hh"
#include "Logger.hh"

#include <sstream>

// #include <nlohmann/json.hpp> // JSON export
#include "../third_party/json-src/single_include/nlohmann/json.hpp" // JSON export

static httplib::Server *serverPtr = nullptr;

ProgressTracker::ProgressTracker(int totalJobs)
    : total(totalJobs), startTime(chrono::steady_clock::now()) {}

void ProgressTracker::pause() { isPaused = true; }
void ProgressTracker::resume() { isPaused = false; }

void ProgressTracker::setLogInterval(int jobCount)
{
    logInterval = jobCount > 0 ? jobCount : 1;
}

void ProgressTracker::setHighlightLatency(int thresholdMs)
{
    highlightThreshold = thresholdMs;
}

void ProgressTracker::setEnableColor(bool enable)
{
    enableColor = enable;
}

string ProgressTracker::colorText(const string &text, const string &colorCode) const
{
    return enableColor ? ("\033[" + colorCode + "m" + text + "\033[0m") : text;
}

void ProgressTracker::markJobDone(int latencyMs)
{
    latencies.push_back(latencyMs);
    int current = ++done;

    if (highlightThreshold > 0 && latencyMs > highlightThreshold)
    {
        Logger::dualSafeLog(colorText("[!!!] High latency job: " + to_string(latencyMs) + "ms", "31")); // red
    }

    if ((current - lastLoggedDone) >= logInterval && !isPaused)
    {
        updateProgress();
        lastLoggedDone = current;
    }
}

void ProgressTracker::markJobDoneWithCategory(const string &category, int latencyMs)
{
    {
        lock_guard<mutex> lock(categoryMetrics[category].mtx_);
        categoryMetrics[category].latencies.push_back(latencyMs);

        auto &metric = categoryMetrics[category];
        if (latencyMs < metric.minLatency)
            metric.minLatency = latencyMs;

        if (latencyMs > metric.maxLatency)
            metric.maxLatency = latencyMs;

        metric.count++;
    }

    totalDone++;

    if (highlightThreshold > 0 && latencyMs > highlightThreshold)
    {
        Logger::dualSafeLog(colorText("[!!!] High latency job (" + category + "): " + to_string(latencyMs) + "ms", "31"));
    }
}

void ProgressTracker::updateProgress()
{
    if (isPaused)
        return;

    float progress = total == 0 ? 1.0f : static_cast<float>(done) / total;
    if (progress > 1.0f)
        progress = 1.0f;

    int avg = averageLatency();
    string etaStr = formatETA();

    stringstream ss;
    ss << "Progress: " << static_cast<int>(progress * 100)
       << "% | ETA: " << etaStr
       << " | Avg latency: " << avg << "ms";

    Logger::dualSafeLog("");
    Logger::dualSafeLog(colorText(ss.str(), "36")); // cyan
    Logger::dualSafeLog("");
}

void ProgressTracker::finish()
{
    auto endTime = chrono::steady_clock::now();
    int totalTime = chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count();
    int avg = averageLatency();

    Logger::dualSafeLog("");
    Logger::dualSafeLog("         All workers finished. Total job done: " + to_string(done));
    Logger::dualSafeLog("");
    Logger::dualSafeLog("  Average job latency: " + to_string(avg) + "ms");
    Logger::dualSafeLog("");
    Logger::dualSafeLog(" Total execution time: " + to_string(totalTime) + "ms");
    Logger::dualSafeLog("");
}

int ProgressTracker::averageLatency() const
{
    if (latencies.empty())
        return 0;
    int sum = 0;
    for (int l : latencies)
        sum += l;
    return sum / static_cast<int>(latencies.size());
}

string ProgressTracker::formatETA() const
{
    if (done == 0 || done >= total)
        return done >= total ? "0s" : "N/A";
    int avg = averageLatency();
    int remaining = total - done;
    int etaMs = avg * remaining;
    int etaSec = (etaMs + 999) / 1000;
    return to_string(etaSec) + "s";
}

string ProgressTracker::exportSummaryJSON() const
{
    using json = nlohmann::json;
    auto endTime = chrono::steady_clock::now();
    int totalTime = chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count();

    json j;
    j["total_jobs"] = total;
    j["completed_jobs"] = done.load();
    j["average_latency_ms"] = averageLatency();
    j["total_execution_time_ms"] = totalTime;
    j["paused"] = isPaused.load();

    json categoriesJson = json::object();
    for (const auto &[category, metric] : categoryMetrics)
    {
        lock_guard<mutex> lock(metric.mtx_);
        categoriesJson[category] = {
            {"count", metric.count.load()},
            {"average_latency_ms", metric.latencies.empty() ? 0 : accumulate(metric.latencies.begin(), metric.latencies.end(), 0) / (int)metric.latencies.size()},
            {"min_latency_ms", metric.minLatency},
            {"max_latency_ms", metric.maxLatency}};
    }
    j["categories"] = categoriesJson;

    return j.dump(4); // pretty print
}

string ProgressTracker::exportPrometheus() const
{
    stringstream ss;

    // Define standard Prometheus bucket (ms)
    vector<int> buckets = {50, 100, 250, 500, 1000};

    int expectedTotal = 0;

    // Browse each category group
    for (const auto &[category, metric] : categoryMetrics)
    {
        lock_guard<mutex> lock(metric.mtx_);

        // Calculate total latency and count buckets
        map<int, int> bucketCounts;
        int latencySum = 0;
        int count = metric.count.load();

        for (int l : metric.latencies)
        {
            latencySum += l;
            for (int b : buckets)
            {
                if (l <= b)
                    bucketCounts[b]++;
            }
        }

        expectedTotal += count;

        // Print HELP and TYPE (optional)
        ss << "# HELP job_latency_bucket Histogram of job latency in ms for category " << category << "\n";
        ss << "# TYPE job_latency_bucket histogram\n";
        ss << "# HELP job_latency_sum Sum of job latency in ms for category " << category << "\n";
        ss << "# TYPE job_latency_sum gauge\n";
        ss << "# HELP job_latency_count Count of jobs processed for category " << category << "\n";
        ss << "# TYPE job_latency_count counter\n";

        // Print bucket counts
        for (int b : buckets)
        {
            ss << "job_latency_bucket{category=\"" << category << "\",le=\"" << b << "\"} "
               << bucketCounts[b] << "\n";
        }

        // +Inf bucket
        ss << "job_latency_bucket{category=\"" << category << "\",le=\"+Inf\"} " << count << "\n";

        // Sum & Count
        ss << "job_latency_sum{category=\"" << category << "\"} " << latencySum << "\n";
        ss << "job_latency_count{category=\"" << category << "\"} " << count << "\n";
    }

    // Overall metrics
    ss << "job_total_done " << totalDone.load() << "\n";
    ss << "job_total_expected " << expectedTotal << "\n";

    return ss.str();
}

string ProgressTracker::exportJSON() const
{
    using json = nlohmann::json;
    json root;
    int expectedTotal = 0;

    root["total_done"] = totalDone.load();
    json categories = json::object();

    for (const auto &[category, metric] : categoryMetrics)
    {
        lock_guard<mutex> lock(metric.mtx_);

        int sum = 0;
        for (int l : metric.latencies)
            sum += l;

        int count = metric.count.load();
        expectedTotal += count;

        double avg = metric.latencies.empty() ? 0.0 : static_cast<double>(sum) / metric.latencies.size();

        categories[category] = {
            {"job_count", count},
            {"average_latency_ms", avg},
            {"min_latency_ms", metric.minLatency},
            {"max_latency_ms", metric.maxLatency}};
    }

    root["total_expected"] = expectedTotal;
    root["categories"] = categories;
    return root.dump(4); // pretty print
}

void ProgressTracker::startHTTPServer(int port)
{
    if (serverPtr != nullptr)
        return; // server is already running, avoid restarting

    serverPtr = new httplib::Server();

    // Endpoint /metrics returns Prometheus format string
    serverPtr->Get("/metrics", [this](const httplib::Request &, httplib::Response &res)
                   { res.set_content(this->exportPrometheus(), "text/plain; version=0.0.4"); });

    // Run the server on a separate thread, not main block
    thread([port]()
           { 
printf("[ProgressTracker] HTTP server started on port %d\n", port); 
serverPtr->listen("0.0.0.0", port); })
        .detach();
}
