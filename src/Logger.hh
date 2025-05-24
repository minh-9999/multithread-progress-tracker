
#pragma once

#include <fstream>
#include <mutex>
#include <string>

using namespace std;

class Logger
{
public:
    static void init(const string &logFilePath);
    static void log(const string &message);
    static void dualSafeLog(const string &message);
    static void logJSON(const string &event, const string &status, int latency, int attempt);

    static void flush();

private:
    static ofstream logFile;
    static mutex logMutex;
};
