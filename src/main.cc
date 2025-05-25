#include <cstdlib>
#include <iostream>
#include <thread>
#include <random>
#include <fstream>
#include <format>

#include "ProgressTracker.hh"
#include "Logger.hh"

#include <filesystem>

namespace fs = filesystem;

void notifyResult(int method)
{
    fs::path script_dir = "script";

    if (method == 1)
    {
        char *webhook = nullptr;
        size_t len = 0;
        errno_t err = _dupenv_s(&webhook, &len, "SLACK_WEBHOOK_URL");

        if (err != 0 || webhook == nullptr || len == 0)
        {
            fprintf(stderr, "[Abort] SLACK_WEBHOOK_URL is not set in environment.\n");
            if (webhook)
                free(webhook);

            return;
        }

        string cmd = "cd " + script_dir.string() + " && set SLACK_WEBHOOK_URL=" +
                     string(webhook) + " && python notify.py";

        free(webhook);

        int ret = system(cmd.c_str());

        if (ret != 0)
            fprintf(stderr, "Error: notify.py returned %d\n", ret);
    }

    else if (method == 2)
    {
        string cmd = "cd " + script_dir.string() + " && send_slack.cmd job_summary.json";
        int ret = system(cmd.c_str());
        if (ret != 0)
            fprintf(stderr, "Error: send_slack.cmd returned %d\n", ret);
    }

    else
    {
        fprintf(stderr, "Unsupported notify method: %d\n", method);
    }
}

int main()
{
    // Step 1: Init the logger first
    Logger::init("job_log.txt");
    Logger::dualSafeLog("==== Job Dispatcher Started ===");

    // Step 2: User chooses how to send results
    int choice = 0;
    while (true)
    {
        cout << "\n\t Select the form of sending results: \n";
        cout << " 1. Python script \n";
        cout << " 2. Slack webhook \n";
        cout << "\n\t> ";
        cin >> choice;

        if (cin.fail() || choice < 1 || choice > 2)
        {
            cin.clear();
            cin.ignore(1000, '\n');
            cout << "\n Invalid choice. Please enter 1 or 2.\n";
        }
        else
        {
            // cout << "\n";
            break;
        }
    }

    // Step 3: Setup tracker
    const int totalJobs = 20;
    ProgressTracker tracker(totalJobs);

    // Start HTTP metrics server on port 8080
    tracker.startHTTPServer(8080);

    tracker.setEnableColor(true);
    tracker.setHighlightLatency(250); // ms
    tracker.setLogInterval(3);        // log every 3 jobs

    const int MAX_RETRIES = 3;
    const int LATENCY_THRESHOLD = 300;

    random_device rd;
    mt19937 rng(rd());
    uniform_int_distribution<int> dist(50, 400);

    for (int i = 0; i < totalJobs; ++i)
    {
        int retries = 0;
        int latency = 0;

        while (retries < MAX_RETRIES)
        {
            latency = dist(rng);
            this_thread::sleep_for(chrono::milliseconds(latency));

            if (latency <= LATENCY_THRESHOLD)
                break;

            Logger::dualSafeLog("Job " + to_string(i + 1) + " latency too high (" + to_string(latency) + " ms), retrying...");
            ++retries;
        }

        tracker.markJobDone(latency);
    }

    tracker.finish();

    // Step 4: Write JSON to file
    fs::path script_dir = "script";
    string json = tracker.exportSummaryJSON();
    fs::path output_path = script_dir / "job_summary.json";
    ofstream out(output_path);
    out << json;
    out.close();

    auto end = chrono::system_clock::now();
    Logger::dualSafeLog("=== Job finished at " + format("{:%Y-%m-%d %H:%M:%S}", end));
    Logger::dualSafeLog("Summary exported to job_summary.json");
    Logger::dualSafeLog("Total jobs: " + to_string(totalJobs));
    Logger::dualSafeLog("Latency threshold: " + to_string(LATENCY_THRESHOLD) + " ms");

    // Step 5: Send notification
    notifyResult(choice);
}