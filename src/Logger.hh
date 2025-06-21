
#pragma once

#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>

using namespace std;

enum class LogLevel
{
    Debug,
    Info,
    Warn,
    Error
};

struct LogMessage
{
    string event, status;
    int latency, attempt;
    LogLevel level;
    thread::id threadId;
    chrono::system_clock::time_point timestamp;
};
class Logger
{
public:
    static Logger &instance();

    void start(const std::string &filename, bool truncate = true);
    void stop();

    // Logger(const string &filename, bool truncate);

    // ~Logger();

    static string timestamps();

    // static void init(const string &logFilePath);

    static string escapeJSONString(const string &input);

    // static void log(const string &message);
    static void log(LogLevel level, const string &event, const string &status, int latency, int attempt);

    static void dualSafeLog(const string &message);
    static void logJSON(const string &event, const string &status, int latency, int attempt);

    static void flush();

    static void workerThread();

    static string logLevelToString(LogLevel level);

    void logWorker(); // background thread

    void flushBatchToConsole(const vector<LogMessage> batch);
    void flushBatchToFile(const vector<LogMessage> batch);

    inline static unordered_map<LogLevel, int> levelCount; // use in ProgressTracker

private:
    Logger() = default;
    // Logger();
    ~Logger();

    Logger(const Logger &) = delete;
    Logger &operator=(const Logger &) = delete;

    inline static queue<LogMessage> messageQueue; // intermediate buffer
    inline static mutex queueMutex;
    inline static condition_variable cv;
    inline static atomic<bool> stopFlag = false;
    inline static atomic<bool> isReady = false;

    inline static thread worker;

    // Write log to file (only 1 logger file)
    inline static ofstream logFile;
    inline static mutex logMutex;
    inline static mutex coutMutex;

    // Mapping thread::id → thread index (for logger or display)
    inline static unordered_map<thread::id, int> threadIdMap;
    inline static atomic<int> threadCounter{1}; // Start from 1 for easier debugging than thread #0
    inline static mutex threadMapMutex;

    inline static atomic<bool> running;
    atomic<bool> stopLogger = false;
};
