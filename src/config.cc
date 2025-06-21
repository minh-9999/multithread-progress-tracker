#include "config.hh"
#include "Job.hh"
#include "WorkStealing.hh"
#include "JobFactory.hh"

#include <filesystem>
#include <future>
#include <iostream>

namespace fs = filesystem;

static mutex io_mutex;

string escapeEnvVar(const string &input)
{
    string escaped;
    for (char c : input)
    {
        if (c == '&' || c == '|' || c == '>' || c == '<' || c == '%' || c == '"')
        {
            escaped += '^'; // escape for cmd.exe
        }

        escaped += c;
    }

    return escaped;
}

int selectNotificationMethod()
{
    int choice = 0;

    while (true)
    {
        {
            lock_guard<mutex> lock(io_mutex);
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
    }

    return choice;
}

void setupTracker(ProgressTracker &tracker)
{
    tracker.startHTTPServer(8080);
    tracker.setEnableColor(true);
    tracker.setHighlightLatency(250);
    tracker.setLogInterval(3);
}

/*
run a series of jobs, monitor progress, handle retries, and log
*/
Task<void> runMainTasks(ProgressTracker &tracker, Logger &log, int totalJobs, int maxRetries, int latencyThreshold)
{
    for (int i = 0; i < totalJobs; ++i)
    {
        int retries = 0; // count current job retries

        // result.first: latency,
        // result.second: log level (Info, Warn, Error...)
        pair<int, LogLevel> result;

        while (retries < maxRetries)
        {
            // Call coroutine simulateTask to execute job
            result = co_await simulateTask(i + 1, log);

            // If the job has a delay less than or equal to the allowed threshold → success, exit the retry loop
            if (result.first <= latencyThreshold)
                break;

            log.dualSafeLog("Job " + std::to_string(i + 1) + " latency too high (" + std::to_string(result.first) + " ms), retrying...");
            ++retries;
        }

        // Mark job complete
        tracker.markJobDoneWithCategory("main", result.first, result.second);
    }

    co_return;
}

void runThreadPoolTasks(Logger &log)
{
    // Create a thread pool with 4 worker threads.
    /*
        This thread pool supports "work-stealing" mechanism:
        when a thread is idle, it will automatically take work from the queue of another thread.
    */

    WorkStealingThreadPool pool(4);

    // Send 10 tasks to thread pool
    for (int i = 1; i <= 10; ++i)
    {
        pool.enqueue(wrapAsTask([=, &log]()
                                { simulateTask(i, log); }));
        /*
        ensures that 'i' is "frozen" at lambda creation — that is, each lambda will have its own copy of 'i'
        */
    }

    pool.waitAll();
    pool.printStatus();
}

void runPostProcessingJobs()
{
    auto start = chrono::steady_clock::now();

    // Create a job to initialize the database (if it does not exist yet)
    Job dbJob = createInitDatabaseJob();
    // Create job to generate summary report
    Job reportJob = createGenerateReportJob();
    // Create a job to delete temporary files after processing is complete
    Job cleanupJob = createCleanupTempFilesJob();

    // Execute post-processing jobs in turn
    // dbJob.execute();
    // reportJob.execute();
    // cleanupJob.execute();

    future<void> f1 = async(launch::async, [&]
                            { dbJob.execute(); });

    future<void> f2 = async(launch::async, [&]
                            { reportJob.execute(); });

    future<void> f3 = async(launch::async, [&]
                           { cleanupJob.execute(); });

    // Wait for jobs to finish
    f1.get();
    f2.get();
    f3.get();

    auto end = chrono::steady_clock::now();
    // Record the total running time for all sub-jobs
    logElapsedTime("Extra jobs elapsed", start, end);
}

void sendNotification(int method)
{
    Logger::instance().dualSafeLog("All jobs done. Notifying user...");

    notifyResult(method);

    Logger::instance().dualSafeLog("Notification sent. Program exiting cleanly.");
}

pair<int, LogLevel> simulateTasks(int id, Logger &logger)
{
    int latency = 100 * id;
    logger.log(LogLevel::Info, "Start job " + to_string(id), "processing", latency, id);
    this_thread::sleep_for(chrono::milliseconds(latency));

    if (id % 2 == 0)
    {
        logger.log(LogLevel::Warn, "Job " + to_string(id), "slow response", latency, 1);
        return {latency, LogLevel::Warn};
    }
    else
    {
        logger.log(LogLevel::Error, "Job " + to_string(id), "timeout", latency, 3);
        return {latency, LogLevel::Error};
    }
}

/*
simulate a task with delay
*/
Task<pair<int, LogLevel>> simulateTask(int id, Logger &logger)
{
    int latency = 100 * id; // Each job will have a delay of 100ms * id

    logger.log(LogLevel::Info, "Start job " + to_string(id), "processing", latency, id);

    // Pause to simulate processing
    co_await sleepAsync(chrono::milliseconds(latency));

    if (id % 2 == 0)
    {
        // Log the Warn level for slow response, and return the result {latency, Warn}
        logger.log(LogLevel::Warn, "Job " + to_string(id), "slow response", latency, 1);
        co_return {latency, LogLevel::Warn};
    }

    else
    {
        // Log the Error level because of timeout, and return {latency, Error}
        logger.log(LogLevel::Error, "Job " + to_string(id), "timeout", latency, 3);
        co_return {latency, LogLevel::Error};
    }
}

/*
simulates the processing of a task with random delay
*/
future<pair<int, LogLevel>> simulateTaskAsync(int id, Logger &logger)
{
    // launch lambda in a separate thread
    return async(launch::async, [id, &logger]() -> pair<int, LogLevel>
                 {
        // Each thread has its own generator.
        static thread_local mt19937 rng(random_device{}());
        // latency will be between 50–400ms → simulate processing time
        uniform_int_distribution<int> dist(50, 400);
        int latency = dist(rng);

        logger.log(LogLevel::Info, "Start job " + to_string(id), "processing", latency, id);

        // Processing simulation
        this_thread::sleep_for(chrono::milliseconds(latency));

        LogLevel level = LogLevel::Info;
        if (latency > 300) 
        {
            level = (id % 3 == 0) ? LogLevel::Error : LogLevel::Warn;
            string msg = (level == LogLevel::Error) ? "timeout" : "slow response";
            logger.log(level, "Job " + to_string(id), msg, latency, 1);
        }

        return {latency, level}; });
}

/*
simulate a task with delay
*/
Task<pair<int, LogLevel>> simulateTaskCoroutine(int id, Logger &logger)
{
    int latency = 100 * id; // Each job will have a delay of 100ms * id
    logger.log(LogLevel::Info, "Start job " + to_string(id), "processing", latency, id);

    /*
    suspend_always is an awaitable that always pauses the coroutine when co_await is encountered.
    */
    co_await suspend_always{}; // fake async point

    // Pause to simulate processing (Block the current thread for a millisecond latency period)
    this_thread::sleep_for(chrono::milliseconds(latency));

    if (id % 2 == 0)
    {
        logger.log(LogLevel::Warn, "Job " + to_string(id), "slow response", latency, 1);
        co_return {latency, LogLevel::Warn};
    }

    else
    {
        logger.log(LogLevel::Error, "Job " + to_string(id), "timeout", latency, 3);
        co_return {latency, LogLevel::Error};
    }
}

Task<std::pair<int, LogLevel>> simulateTaskAsyncCoroutine(int id, Logger &logger)
{
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(50, 400);
    int latency = dist(rng); // Random delay

    logger.log(LogLevel::Info, "Start job " + std::to_string(id), "processing", latency, id);

    // Asynchronous stop: the coroutine will pause, not block the thread
    co_await sleepAsync(std::chrono::milliseconds(latency));

    LogLevel level = LogLevel::Info;

    if (latency > 300)
    {
        level = (id % 3 == 0) ? LogLevel::Error : LogLevel::Warn;
        std::string msg = (level == LogLevel::Error) ? "timeout" : "slow response";
        logger.log(level, "Job " + std::to_string(id), msg, latency, 1);
    }

    co_return {latency, level};
}

void notifyResult(int method)
{
    Logger &log = Logger::instance();

    fs::path script_dir = "script";
    if (!fs::exists(script_dir))
    {
        log.dualSafeLog("Script directory not found: " + script_dir.string());

        return;
    }

    if (method == 1)
    {
        // char *webhook = nullptr;
        // size_t len = 0;
        // errno_t err = _dupenv_s(&webhook, &len, "SLACK_WEBHOOK_URL");

        // if (err != 0 || webhook == nullptr || len == 0)
        // {
        //     fprintf(stderr, "[Abort] SLACK_WEBHOOK_URL is not set in environment.\n");
        //     if (webhook)
        //         free(webhook);

        //     return;
        // }

        // string safeWebhook = escapeEnvVar(webhook);
        // string safeWebhook = webhook;
        // string cmd = "cd " + script_dir.string() + " && set SLACK_WEBHOOK_URL=" +
        //              string(webhook) + " && python notify.py";

        // string cmd = "cd " + script_dir.string() +
        //              " && python notify.py \"" + string(webhook) + "\"";

        // string cmd = "cd " + script_dir.string() + " && set SLACK_WEBHOOK_URL=" +
        //              "\"" + safeWebhook + "\"" + " && python notify.py ";

        // free(webhook);

        fs::path script_dir = fs::absolute("script");
        fs::path script_path = script_dir / "notify.py";

        string command = "cmd /C \"python \"" + script_path.string() + "\"\"";
        int ret = system(command.c_str());

        log.dualSafeLog("Executing command: " + command);
        log.dualSafeLog("Notification script completed. See notify_log.txt for details.");

        if (ret != 0)
        {
            // fprintf(stderr, "Error: notify.py returned %d\n", ret);
            log.dualSafeLog("❌ notify.py exited with code " + to_string(ret));
        }
    }

    else if (method == 2)
    {
        string cmd = "cd " + script_dir.string() + " && send_slack.cmd job_summary.json";
        int ret = system(cmd.c_str());
        if (ret != 0)
        {
            // fprintf(stderr, "Error: send_slack.cmd returned %d\n", ret);

            log.dualSafeLog("❌ send_slack.cmd exited with code " + to_string(ret));
        }
    }

    else
    {
        // fprintf(stderr, "Unsupported notify method: %d\n", method);

        log.dualSafeLog(" ⚠️ Unsupported notify method: " + to_string(method));
    }
}

void logElapsedTime(const string &label, chrono::steady_clock::time_point start, chrono::steady_clock::time_point end)
{
    auto elapsed = chrono::duration_cast<chrono::seconds>(end - start).count();

    Logger::instance().dualSafeLog(label + ": " + to_string(elapsed) + " seconds");
}
