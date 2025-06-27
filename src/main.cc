
#include <thread>
#include <random>
#include <fstream>
#include <format>
#include <filesystem>
#include <iostream>

#include "ProgressTracker.hh"
#include "WorkStealing.hh"
#include "adaptive_task_graph.hh"


#include "config.hh"

namespace fs = filesystem;

mutex io_mutex;

int main()
{
    // Start measuring the total running time of the entire program
    auto overallStart = chrono::steady_clock::now();

    // ======== Step 1: Initialize the logging system ========
    // Logger::init("job_log.txt");

    // Get the current timestamp to name the log file
    auto timestamp = format("{:%Y%m%d_%H%M%S}", chrono::system_clock::now());

    // Get the singleton instance of Logger
    Logger &log = Logger::instance();
    // Start a separate thread to handle background logging
    thread loggerThread(&Logger::logWorker, &log);

    // Initialize log file with name and timestamp attached
    log.start("job_log_" + timestamp + ".txt");
    // Record program startup log
    log.dualSafeLog("==== Job Dispatcher Started ===");

    // ======== Step 2: User chooses method to receive results ========
    int choice = selectNotificationMethod();

    // ======== Step 3: Initialize progress monitoring ========
    const int totalJobs = 20; // Total number of jobs to execute
    ProgressTracker tracker(totalJobs);

    // Start an HTTP server on port 8080 to output monitoring information (Prometheus/Grafana, etc.)
    setupTracker(tracker);

    // Configure maximum retries and latency threshold for each job
    const int MAX_RETRIES = 3;
    const int LATENCY_THRESHOLD = 300;

    // Initialize RNG (Random Number Generator) to simulate job latency
    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<int> dist(50, 400);

    // Call the function to execute the main tasks (using thread pool or async)
    // auto task = runMainTasks(tracker, log, totalJobs, MAX_RETRIES, LATENCY_THRESHOLD);
    // task.wait();

    AdaptiveTaskGraph graph;

    // Tạo N job task dưới dạng coroutine wrapped trong Task<void>
    for (int i = 0; i < totalJobs; ++i)
    {
        // Wrap từng simulateTask vào Task<void>
        auto t = std::make_shared<Task<void>>(wrapAsTask([=, &log, &tracker]() -> Task<void>
                                                         {
        int retries = 0;
        pair<int, LogLevel> result;

        while (retries < MAX_RETRIES)
        {
            result = co_await simulateTask(i + 1, log);
            if (result.first <= LATENCY_THRESHOLD)
                break;
            log.dualSafeLog("Job " + std::to_string(i + 1) + " latency too high (" + std::to_string(result.first) + " ms), retrying...");
            ++retries;
        }

        tracker.markJobDoneWithCategory("main", result.first, result.second);
        co_return; }));

        graph.addTask(t);
    }

    // Chạy toàn bộ graph (bên trong dùng thread pool tự điều chỉnh)
    graph.execute();
    tracker.finish();

    // Print progress summary by completion level
    tracker.printLevelSummary();
    runThreadPoolTasks(log);

    // ======== Step 4: Write the results to JSON file ========
    fs::path script_dir = "script";
    fs::path output_path = script_dir / "job_summary.json";
    json result = tracker.exportSummaryJSON();
    cout << result.dump(4); // Print to screen
    ofstream out(output_path);
    out << result.dump(4); // Write to file
    // cout << "\n\n [Debug] JSON written to: " << fs::absolute(output_path) << endl;
    out.flush(); // Make sure all content is written
    out.close();

    // Get the program end timestamp
    auto end = chrono::system_clock::now();

    // Log the end and summary information
    log.dualSafeLog("\n === Job finished at " + format("{:%Y-%m-%d %H:%M:%S}", end) + "\n");
    log.dualSafeLog("Summary exported to job_summary.json");
    log.dualSafeLog("Total jobs: " + to_string(totalJobs));
    log.dualSafeLog("Latency threshold: " + to_string(LATENCY_THRESHOLD) + " ms");

    // ======== Step 5: Send result notification to user ========

    // Perform post-processing tasks: initialize DB, generate report, clean up temporary files
    runPostProcessingJobs();

    // Send notification according to user's initial selection
    sendNotification(choice);

    log.stop();
    // Wait for the logger thread to finish before exiting the program
    loggerThread.join();
}