
#include "Logger.hh"
#include "log_utils.h"

#include <chrono>
#include <format>
#include <iomanip> // put_time

#include <iostream>
#include <sstream>

static mutex io_mutex;
static mutex file_mutex;

Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::start(const string &filename, bool truncate)
{
    stopFlag = false;
    isReady = false;
    running = true;
    // Create a background thread (workerThread) to process logs (read from messageQueue and write to file)
    worker = thread(&Logger::workerThread);

    // Must lock to safely open log file before any log attempts are made.
    // Otherwise, race condition may cause segmentation fault.
    {
        lock_guard<mutex> lock(logMutex);
        // If truncate = true then open the overwrite file (ios::trunc), otherwise write concatenate (ios::app)
        ios_base::openmode mode = truncate ? (ios::out | ios::trunc) : (ios::app);

        logFile.open(filename, mode);

        if (!logFile.is_open())
            throw runtime_error("Cannot open log file");
    }

    // Wait until worker thread signals it is ready to accept log messages.
    // Prevents race condition where main thread logs before logger is initialized.
    {
        unique_lock<mutex> lock(queueMutex);

        // wait until the workerThread thread reports that the logger is ready to receive logs (isReady = true)
        cv.wait(lock, []
                { return isReady.load(); });
    }

    // Write a first log line indicating that the job has started
    auto now = chrono::system_clock::now();
    dualSafeLog("=== Job started at " + format("{:%Y-%m-%d %H:%M:%S}", now));
}

void Logger::stop()
{
    {
        lock_guard<mutex> lock(queueMutex);
        stopFlag = true;
        running = false;
    }

    cv.notify_all();

    if (worker.joinable())
        worker.join();

    if (logFile.is_open())
        logFile.close();
}

// Logger::Logger() : running(true) {}

Logger::~Logger()
{
    stop(); // ensure worker thread stops and joins

    // Close log file safely
    {
        lock_guard<mutex> lock(logMutex);

        if (logFile.is_open())
            logFile.close();
    }
}

string Logger::timestamps()
{
    auto now = chrono::system_clock::now();
    time_t t = chrono::system_clock::to_time_t(now);
    tm tm;

#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm, &t);

#else
    localtime_r(&t, &tm);
#endif

    ostringstream oss;
    oss << "[" << put_time(&tm, "%Y-%m-%d %H:%M:%S") << "]";
    return oss.str();
}

// Logger::Logger(const string &filename, bool truncate)
// {
//     ios_base::openmode mode = truncate ? (ios::out | ios::trunc) : (ios::app);
//     {
//         lock_guard<mutex> lock(logMutex);
//         logFile.open(filename, mode);

//         if (!logFile.is_open())
//         {
//             throw runtime_error("Cannot open log file");
//         }
//     }

//     stopFlag = false;
//     // worker = thread(&Logger::workerThread);

//     // auto now = chrono::system_clock::now();
//     // dualSafeLog("=== Job started at " + format("{:%Y-%m-%d %H:%M:%S}", now));
// }

// Logger::~Logger()
// {
//     stopFlag = true;
//     cv.notify_all();

//     if (worker.joinable())
//         worker.join();

//     if (logFile.is_open())
//         logFile.close();
// }

// void Logger::init(const string &logFilePath)
// {
//     {
//         lock_guard<mutex> lock(logMutex);
//         // for write and Delete all old content
//         logFile.open(logFilePath, ios::out | ios::trunc);
//     }

//     auto now = chrono::system_clock::now();
//     // time_t now_c = chrono::system_clock::to_time_t(now);
//     // dualSafeLog("=== Job started at " + string(ctime(&now_c)));

//     dualSafeLog("=== Job started at " + format("{:%Y-%m-%d %H:%M:%S}", now));
// }

string Logger::escapeJSONString(const string &input)
{
    ostringstream oss;

    for (char c : input)
    {
        switch (c)
        {
        case '\"':
            oss << "\\\""; // double quote → \"
            break;

        case '\\':
            oss << "\\\\"; // backslash → \\
            break;

        case '\b':
            oss << "\\b";
            break;

        case '\f':
            oss << "\\f"; // form feed → \f
            break;

        case '\n':
            oss << "\\n";
            break;

        case '\r':
            oss << "\\r"; // carriage return → \r
            break;

        case '\t':
            oss << "\\t";
            break;

        default:
            // If the character is control (ASCII < 0x20) or not displayable (ASCII > 0x7E)
            if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E)
                oss << "\\u" << hex << setw(4) << setfill('0') << (int)(unsigned char)c;
            else
                oss << c;
        }
    }

    return oss.str();
}

// void Logger::log(const string &message)
// {
//     lock_guard<mutex> lock(logMutex);
//     if (logFile.is_open())
//         logFile << message << endl;
// }

/*
asynchronous logging system
*/
void Logger::log(LogLevel level, const string &event, const string &status, int latency, int attempt)
{
    LogMessage msg{event,
                   status,
                   latency,
                   attempt,
                   level,
                   this_thread::get_id(), // ID of the current thread
                   chrono::system_clock::now()};

    {
        lock_guard<mutex> lock(queueMutex);
        messageQueue.push(msg);
    }

    cv.notify_one();
}

void Logger::dualSafeLog(const string &message)
{
    string full = "\n" + timestamps() + "  ===  " + message;

    // Locking cout separately to prevent interleaved output in multi-threaded environment
    {
        // lock_guard<mutex> coutLock(coutMutex);
        lock_guard<mutex> lock(g_logMutex);
        cout << full << endl;
    }

    // Locking log file separately to ensure safe concurrent file access
    {
        lock_guard<mutex> fileLock(logMutex);

        if (!running || !logFile.is_open())
            return;

        logFile << full << endl;
    }
}

void Logger::logJSON(const string &event, const string &status, int latency,
                     int attempt)
{
    lock_guard<mutex> lock(logMutex);
    auto now = chrono::system_clock::now();
    time_t now_c = chrono::system_clock::to_time_t(now);
    tm tm_struct;

#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm_struct, &now_c);
#else
    localtime_r(&now_c, &tm_struct);
#endif

    ostringstream timestampStream;
    timestampStream << put_time(&tm_struct, "%Y-%m%d %H:%M%S");
    string timeStamp = timestampStream.str();
    // string threadId = to_string(hash<thread::id>{}(this_thread::get_id()));

    thread::id thisId = this_thread::get_id();
    int threadIndex;

    {
        lock_guard<mutex> mapLock(threadMapMutex);
        auto it = threadIdMap.find(thisId);

        if (it == threadIdMap.end())
        {
            threadIndex = threadCounter++;
            threadIdMap[thisId] = threadIndex;
        }
        else
        {
            threadIndex = it->second;
        }
    }

    string threadLabel = "thread#" + to_string(threadIndex);

    if (!logFile.is_open())
        return;

    logFile << "{ "
            << "\"timestamp\": \"" << timestamps() << "\", "
            << "\"thread_id\": \"" << threadLabel << "\", "
            << "\"event\": \"" << escapeJSONString(event) << "\", "
            << "\"status\": \"" << escapeJSONString(status) << "\", "
            << "\"latency_ms\": " << latency << ", "
            << "\"attempt\": " << attempt << " }" << endl;
}

// Push all log data from buffer to disk,
// ensuring log is not lost if program crashes or is suddenly shut down
void Logger::flush()
{
    /*
    - By default, `ofstream` uses **buffer in RAM** to increase write performance.

    - The `flush()` command will **force** the entire contents of the buffer to
    disk.

    - If `flush()` is not called, the log data **may still be in the buffer**,
    and:
    - Will **only be written** when:

        +Buffer is full
        + `logFile` is closed
        + Or manually call `flush()`

    */

    // Ensure thread-safe flushing
    scoped_lock lock(logMutex);
    // ensure everything in the buffer is written to disk
    logFile.flush();
}

void Logger::workerThread()
{
    // 🚦 STEP 1: Notify that the logger thread has started and is ready
    // This must be done before the main thread continues,
    // otherwise it may call dualSafeLog before worker is ready, causing race/segfault.
    {
        lock_guard<mutex> lock(queueMutex);
        isReady = true;
        cv.notify_all(); // Wake up anyone waiting for logger readiness
    }

    cout << "Logger thread started\n";

    // 🔁 STEP 2: Main processing loop — consume log messages or stop when flagged
    while (true)
    {
        unique_lock<mutex> lock(queueMutex);

        // Efficiently wait for either:
        // - A new message arrives
        // - Or shutdown signal via stopFlag
        cv.wait(lock, []
                { return !messageQueue.empty() || stopFlag.load(); });

        // If we're stopping and nothing to log, exit thread
        if (stopFlag.load() && messageQueue.empty())
            break;

        // Get the next message from the queue (move for efficiency)
        LogMessage lgmsg = std::move(messageQueue.front());
        messageQueue.pop();
        lock.unlock(); // Release lock ASAP to allow producers to push more messages

        int threadIndex;

        // 🧠 STEP 3: Assign readable index to thread ID for cleaner logs
        {
            lock_guard<mutex> mapLock(threadMapMutex);
            auto it = threadIdMap.find(lgmsg.threadId);

            if (it == threadIdMap.end())
            {
                threadIndex = threadCounter++;
                threadIdMap[lgmsg.threadId] = threadIndex;
            }
            else
            {
                threadIndex = it->second;
            }
        }

        // 🕒 STEP 4: Convert timestamp to human-readable string
        time_t now_c = chrono::system_clock::to_time_t(lgmsg.timestamp);
        tm tm_struct;

        // Platform-specific time conversion (thread-safe)
#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm_struct, &now_c);
#else
        localtime_r(&now_c, &tm_struct);
#endif

        ostringstream timestampStream;
        timestampStream << put_time(&tm_struct, "%Y-%m-%d %H:%M:%S");

        // 🧾 STEP 5: Build JSON-formatted log entry
        string threadLabel = "thread#" + to_string(threadIndex);

        string json = "{ "
                      "\"timestamp\": \"" +
                      timestampStream.str() + "\", "
                                              "\"thread_id\": \"" +
                      threadLabel + "\", "
                                    "\"level\": \"" +
                      logLevelToString(lgmsg.level) + "\", "
                                                      "\"event\": \"" +
                      escapeJSONString(lgmsg.event) + "\", "
                                                      "\"status\": \"" +
                      escapeJSONString(lgmsg.status) + "\", "
                                                       "\"latency_ms\": " +
                      to_string(lgmsg.latency) + ", "
                                                 "\"attempt\": " +
                      to_string(lgmsg.attempt) +
                      " }";

        // 💾 STEP 6: Write log to file safely (mutex-protected)
        {
            lock_guard<mutex> lock(logMutex);

            if (logFile.is_open())
                logFile << json << endl;
        }
    }

    // ✅ STEP 7: Graceful shutdown
    cout << "Logger thread exiting\n";
}

string Logger::logLevelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Debug:
        return "DEBUG";

    case LogLevel::Info:
        return "INFO";

    case LogLevel::Warn:
        return "WARN";

    case LogLevel::Error:
        return "ERROR";
    }

    return "UNKNOWN";
}

void Logger::logWorker()
{
    while (running)
    {
        vector<LogMessage> batch;

        {
            unique_lock<mutex> lock(queueMutex);

            // batching non-blocking timeout technique — if logs are low, thread won't sleep forever
            // Waiting for new log or Logger is disabled
            cv.wait_for(lock, chrono::milliseconds(100), [&]
                        { return !messageQueue.empty() || !running; });

            // 📦 Get up to 50 logs at a time
            while (!messageQueue.empty() && batch.size() < 50)
            {
                batch.push_back(std::move(messageQueue.front()));
                messageQueue.pop();
            }
        }

        // ✍️ Batch log processing
        if (!batch.empty())
        {
            flushBatchToConsole(batch);
            flushBatchToFile(batch);
        }
    }

    // Flush remaining logs before exit
    vector<LogMessage> remaining;
    {
        lock_guard<mutex> lock(queueMutex);
        while (!messageQueue.empty())
        {
            remaining.push_back(std::move(messageQueue.front()));
            messageQueue.pop();
        }
    }

    // Flush final remaining log
    if (!remaining.empty())
    {
        flushBatchToConsole(remaining);
        flushBatchToFile(remaining);
    }
}

void Logger::flushBatchToConsole(const vector<LogMessage> batch)
{
    // lock_guard<mutex> lock(io_mutex);
    lock_guard<mutex> lock(g_logMutex);

    for (const auto &msg : batch)
    {
        time_t t = chrono::system_clock::to_time_t(msg.timestamp);
        tm tm;

#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif

        stringstream timeStr;
        timeStr << put_time(&tm, "%Y-%m-%d %H:%M:%S");

        string levelStr;
        switch (msg.level)
        {
        case LogLevel::Info:
            levelStr = "INFO";
            break;

        case LogLevel::Warn:
            levelStr = "WARN";
            break;

        case LogLevel::Error:
            levelStr = "ERR ";
            break;

        case LogLevel::Debug:
            levelStr = "DBG ";
            break;
        }

        cout << "[" << timeStr.str() << "]  "
             << "[" << levelStr << "]  "
             << "[" << msg.event << "]  "
             << "[" << msg.status << "]  "
             << "latency = " << msg.latency << "ms  "
             << "attempt = " << msg.attempt << "  "
             << "thread = " << msg.threadId << "\n";
    }
}

void Logger::flushBatchToFile(const vector<LogMessage> batch)
{
    // lock_guard<mutex> lock(file_mutex);
    lock_guard<mutex> lock(g_logMutex);

    if (!logFile.is_open())
        return;

    for (const auto &msg : batch)
    {
        time_t t = chrono::system_clock::to_time_t(msg.timestamp);
        tm tm;

#if defined(_WIN32) || defined(_WIN64)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif

        stringstream timeStr;
        timeStr << put_time(&tm, "%Y-%m-%d %H:%M:%S");

        string levelStr;
        switch (msg.level)
        {
        case LogLevel::Info:
            levelStr = "INFO";
            break;

        case LogLevel::Warn:
            levelStr = "WARN";
            break;

        case LogLevel::Error:
            levelStr = "ERR ";
            break;

        case LogLevel::Debug:
            levelStr = "DBG ";
            break;
        }

        logFile << "[" << timeStr.str() << "]  "
                << "[" << levelStr << "]  "
                << "[" << msg.event << "]  "
                << "[" << msg.status << "]  "
                << "latency = " << msg.latency << "ms  "
                << "attempt = " << msg.attempt << "  "
                << "thread = " << msg.threadId << "\n";
    }

    logFile.flush();
}