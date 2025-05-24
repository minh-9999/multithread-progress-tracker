#include "benchmark.hh"
#include "Logger.hh"

#include <iostream>

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

    cout << "\n Benchmark complete.\n";
    cout << "CSV:     result/benchmark_result.csv\n";
    cout << "JSON:    result/job_summary.json\n";
    cout << "Metrics: http://localhost:9090/metrics\n";
}