#include "benchmark.hh"

#include "JobDispatcher.hh"
#include "ProgressTracker.hh"

#include <filesystem>

namespace fs = filesystem;

void runBenchmark(int numThreads, int numJobs, int sleepPerJobMs, int &durationMs)
{
  JobDispatcher dispatcher(numThreads);
  atomic<int> done{0};

  // Set up tracker to log and monitor
  ProgressTracker tracker(numJobs);
  tracker.setEnableColor(true);
  tracker.setHighlightLatency(80); // Customize if desired
  tracker.setLogInterval(5);       // Log every 5s
  tracker.startHTTPServer(9090);   // Prometheus-style /metrics

  // Create and dispatch jobs
  for (int i = 0; i < numJobs; ++i)
  {
    auto jobPtr = make_unique<Job>([&, i]()
                                   {
      auto startJob = chrono::steady_clock::now();
      // simulate working time
      this_thread::sleep_for(chrono::milliseconds(sleepPerJobMs + (i % 5) * 5));
      auto endJob = chrono::steady_clock::now();

      int measuredLatency = chrono::duration_cast<chrono::milliseconds>(endJob - startJob).count();

      // Assign log level according to latency threshold
      LogLevel level = LogLevel::Info;
      if (measuredLatency > 150)
        level = LogLevel::Error;

      else if (measuredLatency > 100)
        level = LogLevel::Warn;

      tracker.markJobDoneWithCategory("benchmark", measuredLatency, level);
      done++; });

    dispatcher.dispatch(i % numThreads, std::move(jobPtr));
  }

  auto start = chrono::steady_clock::now();

  // Wait until all jobs are done
  while (done.load() < numJobs)
  {
    this_thread::sleep_for(chrono::milliseconds(10));
  }

  auto end = chrono::steady_clock::now();
  dispatcher.stop(); // Stop thread pool
  tracker.finish();

  // Total execution time
  durationMs = chrono::duration_cast<chrono::milliseconds>(end - start).count();

  tracker.printLevelSummary();

  // Write the resulting JSON file
  string json = tracker.exportSummaryJSON();
  fs::create_directories("result");
  string filename = "result/job_summary_" + to_string(numThreads) + ".json";
  ofstream out(filename);
  out << json;
  out.close();
}
