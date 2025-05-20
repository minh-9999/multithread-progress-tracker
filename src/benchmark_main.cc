#include <fstream>
#include <iostream>
#include <atomic>
#include <chrono>
#include <thread>
#include <filesystem>
#include "JobDispatcher.hh"
#include "ProgressTracker.hh"
#include "Logger.hh"

using namespace std;
namespace fs = filesystem;

void runBenchmark(int numThreads, int numJobs, int sleepPerJobMs, int &durationMs)
{
    JobDispatcher dispatcher(numThreads);
    atomic<int> done = 0;

    // Tracker for progress & latency
    ProgressTracker tracker(numJobs);
    tracker.setEnableColor(true);
    tracker.setHighlightLatency(80); // Customize if desired
    tracker.setLogInterval(5);
    tracker.startHTTPServer(9090); // Prometheus-style /metrics

    for (int i = 0; i < numJobs; ++i)
    {
        dispatcher.dispatch(i % numThreads, Job{[&, i]()
                                                {
                                                    this_thread::sleep_for(chrono::milliseconds(sleepPerJobMs + (i % 5) * 5)); // simulate varied latency
                                                    tracker.markJobDoneWithCategory("benchmark", sleepPerJobMs);
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
    tracker.finish();

    durationMs = chrono::duration_cast<chrono::milliseconds>(end - start).count();

    // Write the resulting JSON file
    string json = tracker.exportSummaryJSON();
    fs::create_directories("result");
    string filename = "result/job_summary_" + to_string(numThreads) + ".json";
    ofstream out(filename);
    out << json;
    out.close();
}

int main()
{
    Logger::init("result/benchmark_log.txt");

    ofstream csv("result/benchmark_result.csv");
    csv << "threads,duration_ms\n";

    for (int k : {1, 2, 4, 8})
    {
        int duration = 0;
        runBenchmark(k, 100, 20, duration);
        csv << k << "," << duration << "\n";
        cout << "[THREADS = " << k << "]  DONE in " << duration << " ms\n";
    }

    cout << "\nBenchmark complete.\n";
    cout << "CSV:     result/benchmark_result.csv\n";
    cout << "JSON:    result/job_summary.json\n";
    cout << "Metrics: http://localhost:9090/metrics\n";
}
