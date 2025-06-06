#include "../third_party/httplib/httplib.h"
#include "ProgressTracker.hh"

#include <sstream>

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

void ProgressTracker::markJobDone(int latencyMs, LogLevel level)
{
    latencySum += latencyMs;
    latencyCount++;

    // {
    //     lock_guard<mutex> lock(latencyMutex);
    //     latencies.push_back(latencyMs);
    // }

    Logger::levelCount[level]++; // statistics by log level
    int current = ++done;        // Update number of completed jobs

    // Log warning if latency is high
    if (highlightThreshold > 0 && latencyMs > highlightThreshold)
    {
        Logger::dualSafeLog(colorText("[!!!] High latency job: " + to_string(latencyMs) + "ms [" + Logger::logLevelToString(level) + "]", "31")); // red
    }

    // Update progress if enough jobs
    if ((current - lastLoggedDone) >= logInterval && !isPaused)
    {
        updateProgress();
        lastLoggedDone = current;
    }
}

/*
This function helps to track detailed data on work performance (by category), including:

- Delay time of each job,
- Number of jobs by log level,
- Min/max latency statistics,
- Total number of completed jobs,
- Warning when detecting jobs with high latency.
*/
void ProgressTracker::markJobDoneWithCategory(const string &category, int latencyMs, LogLevel level)
{
    // Update category statistics
    {
        auto &metric = categoryMetrics[category];
        lock_guard<mutex> lock(metric.mtx_);
        metric.latencies.push_back(latencyMs);

        metric.lvlCount[level]++;

        if (latencyMs < metric.minLatency)
            metric.minLatency = latencyMs;

        if (latencyMs > metric.maxLatency)
            metric.maxLatency = latencyMs;

        metric.count++;
    }

    // Update overall statistics for each category by level
    {
        lock_guard<mutex> lock(levelCountMutex);
        categoryLevelCounts[category][level]++;
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

// int ProgressTracker::averageLatency() const
// {
//     lock_guard<mutex> lock(latencyMutex);

//     if (latencies.empty())
//         return 0;

//     int sum = 0;
//     for (int l : latencies)
//         sum += l;

//     return sum / static_cast<int>(latencies.size());
// }

int ProgressTracker::averageLatency() const
{
    int count = latencyCount.load();

    return count == 0 ? 0 : latencySum.load() / count;
}

/*
Returns a string describing the estimated time remaining to completion (ETA: Estimated Time of Arrival)
*/
string ProgressTracker::formatETA() const
{
    // Check special conditions
    if (done == 0 || done >= total)
        return done >= total ? "0s" : "N/A";

    int avg = averageLatency();        // average delay per job (ms)
    int remaining = total - done;      // amount of work remaining
    int etaMs = avg * remaining;       // Estimated total time remaining (ms)
    int etaSec = (etaMs + 999) / 1000; // convert to seconds, round up

    int minutes = etaSec / 60;
    int seconds = etaSec % 60;

    stringstream ss;
    if (minutes > 0)
        ss << minutes << "m";

    ss << seconds << "s";

    return ss.str();
}

/*
Export all process statistics in JSON format, for logging, API, or debugging:

- Summary of current status (number of jobs, latency, execution time...).
- Summary of statistics for each category.
- Added summary by log level.
*/
json ProgressTracker::exportSummaryJSON()
{
    // Record the current time
    auto endTime = chrono::steady_clock::now();
    int totalTime = chrono::duration_cast<chrono::milliseconds>(endTime - startTime).count();

    // Write general information to JSON
    json j; // The overall JSON contains all the information
    j["total_jobs"] = total;
    j["completed_jobs"] = done.load();
    j["average_latency_ms"] = averageLatency();
    j["total_execution_time_ms"] = totalTime;
    j["paused"] = isPaused.load();

    // initialize a JSON object as an object (i.e. a key-value pair like a dictionary in Python or a map<string, json> in C++)
    json categoriesJson = json::object(); // The child JSON contains only data by category

    // Record information for each category
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
    j["levelSummary"] = exportLevelSummaryJSON();

    // return j.dump(4); // pretty print
    return j;
}

string ProgressTracker::exportPrometheus() const
{
    stringstream ss;

    // Define standard Prometheus bucket (ms)
    // ms latency markers to divide the latency into histogram regions
    vector<int> buckets = {50, 100, 250, 500, 1000};

    // Total expected jobs from all categories
    int expectedTotal = 0;

    // Browse each category group
    for (const auto &[category, metric] : categoryMetrics)
    {
        lock_guard<mutex> lock(metric.mtx_);

        // Calculate total latency and count buckets
        map<int, int> bucketCounts;      // count latency falling into each bucket
        int latencySum = 0;              // total latency of all jobs
        int count = metric.count.load(); // total number of jobs in that category

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

        // Write Prometheus format data
        // Print HELP and TYPE (optional)
        ss << "# HELP job_latency_bucket Histogram of job latency in ms for category " << category << "\n";
        ss << "# TYPE job_latency_bucket histogram\n";
        ss << "# HELP job_latency_sum Sum of job latency in ms for category " << category << "\n";
        ss << "# TYPE job_latency_sum gauge\n";
        ss << "# HELP job_latency_count Count of jobs processed for category " << category << "\n";
        ss << "# TYPE job_latency_count counter\n";

        // Export bucket data
        for (int b : buckets)
        {
            ss << "job_latency_bucket{category=\"" << category << "\",le=\"" << b << "\"} "
               << bucketCounts[b] << "\n";
        }

        // +Inf bucket
        ss << "job_latency_bucket{category=\"" << category << "\",le=\"+Inf\"} " << count << "\n";

        // Export total delay and quantity
        ss << "job_latency_sum{category=\"" << category << "\"} " << latencySum << "\n";
        ss << "job_latency_count{category=\"" << category << "\"} " << count << "\n";
    }

    // Overall data (all categories)
    ss << "job_total_done " << totalDone.load() << "\n";
    ss << "job_total_expected " << expectedTotal << "\n";

    return ss.str();
}

string ProgressTracker::exportJSON() const
{
    json root;             // The original JSON contains all the information
    int expectedTotal = 0; // Total number of jobs recorded in categories

    root["total_done"] = totalDone.load();
    json categories = json::object();

    for (const auto &[category, metric] : categoryMetrics)
    {
        lock_guard<mutex> lock(metric.mtx_);

        int sum = 0; // Total latency
        for (int l : metric.latencies)
            sum += l;

        int count = metric.count.load(); // Number of jobs
        expectedTotal += count;

        // Avoid dividing by zero if there is no data
        double avg = metric.latencies.empty() ? 0.0 : static_cast<double>(sum) / metric.latencies.size();

        // Write data of each category into JSON
        categories[category] = {
            {"job_count", count},
            {"average_latency_ms", avg},
            {"min_latency_ms", metric.minLatency},
            {"max_latency_ms", metric.maxLatency}};
    }

    // Write aggregate data to JSON
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
printf("\n [ProgressTracker] HTTP server started on port %d\n", port); 
serverPtr->listen("0.0.0.0", port); })
        .detach();
}

/*
Print a summary table of log counts by category and level
*/
void ProgressTracker::printLevelSummary()
{
    lock_guard<mutex> lock(levelCountMutex);
    Logger::dualSafeLog("\n\n\t ========================== Log Level Summary ========================== \n");

    for (const auto &[category, levelMap] : categoryLevelCounts)
    {
        Logger::dualSafeLog("Category: " + category);

        for (const auto &[level, count] : levelMap)
        {
            string levelStr;

            switch (level)
            {
            case LogLevel::Info:
                levelStr = "INFO";
                break;

            case LogLevel::Warn:
                levelStr = "WARN";
                break;

            case LogLevel::Error:
                levelStr = "ERROR";
                break;

            default:
                levelStr = "UNKNOWN";
                break;
            }

            Logger::dualSafeLog("  - " + levelStr + ": " + to_string(count));
        }
    }
}

json ProgressTracker::exportLevelSummaryJSON()
{
    lock_guard<mutex> lock(levelCountMutex);
    json j;

    for (const auto &[category, levelMap] : categoryLevelCounts)
    {
        for (const auto &[level, count] : levelMap)
        {
            string levelStr;
            switch (level)
            {
            case LogLevel::Info:
                levelStr = "INFO";
                break;

            case LogLevel::Warn:
                levelStr = "WARN";
                break;

            case LogLevel::Error:
                levelStr = "ERROR";
                break;

            default:
                levelStr = "UNKNOWN";
                break;
            }

            j[category][levelStr] = count;
        }
    }

    return j;
}
