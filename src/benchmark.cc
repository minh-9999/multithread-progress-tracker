#include "benchmark.hh"

#include "JobDispatcher.hh"
#include "ProgressTracker.hh"

#include <fstream>
#include <atomic>
#include <chrono>
#include <thread>
#include <filesystem>


using namespace std;
namespace fs = filesystem;

void runBenchmark(int numThreads, int numJobs, int sleepPerJobMs, int &durationMs)
{
    JobDispatcher dispatcher(numThreads);
    atomic<int> done{0};

    // Tracker for progress & latency
    ProgressTracker tracker(numJobs);
    tracker.setEnableColor(true);
    tracker.setHighlightLatency(80); // Customize if desired
    tracker.setLogInterval(5);
    tracker.startHTTPServer(9090); // Prometheus-style /metrics

    for (int i = 0; i < numJobs; ++i)
    {
        auto jobPtr = make_unique<Job>([&, i]()
                                       {
        this_thread::sleep_for(chrono::milliseconds(sleepPerJobMs + (i % 5) * 5));
        tracker.markJobDoneWithCategory("benchmark", sleepPerJobMs);
        done++; });

        dispatcher.dispatch(i % numThreads, std::move(jobPtr));
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


