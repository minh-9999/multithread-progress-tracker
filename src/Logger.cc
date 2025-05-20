
#include "Logger.hh"
#include <chrono>
#include <format>
#include <iomanip> // put_time

#include <iostream>
#include <sstream>


ofstream Logger::logFile;
mutex Logger::logMutex;

static string timestamp()
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


void Logger::init(const string &logFilePath)
{
    {
        lock_guard<mutex> lock(logMutex);
        logFile.open(logFilePath, ios::out | ios::trunc);
    }

    auto now = chrono::system_clock::now();
    // time_t now_c = chrono::system_clock::to_time_t(now);
    // dualSafeLog("=== Job started at " + string(ctime(&now_c)));

    dualSafeLog("=== Job started at " + format("{:%Y-%m-%d %H:%M:%S}", now));
}

void Logger::log(const string &message)
{
    lock_guard<mutex> lock(logMutex);
    if (logFile.is_open())
        logFile << message << endl;
}

void Logger::dualSafeLog(const string &message)
{
    lock_guard<mutex> lock(logMutex);
    string full = timestamp() + "  ===  " + message;
    cout << full << endl;

    if (logFile.is_open())
        logFile << full << endl;
}
