

#include "ProgressTracker.hh"
#include "task.hh"

#include <future>

using namespace std;

string escapeEnvVar(const string &input);

int selectNotificationMethod();

void setupTracker(ProgressTracker &tracker);

Task<void> runMainTasks(ProgressTracker &tracker, Logger &log, int totalJobs, int maxRetries, int latencyThreshold);

void runThreadPoolTasks(Logger &log);

void runPostProcessingJobs();

void sendNotification(int method);

pair<int, LogLevel> simulateTasks(int id, Logger &logger);

Task<pair<int, LogLevel>> simulateTask(int id, Logger &logger);

future<pair<int, LogLevel>> simulateTaskAsync(int id, Logger &logger);

Task<pair<int, LogLevel>> simulateTaskCoroutine(int id, Logger &logger);

void notifyResult(int method);

void logElapsedTime(const string &label, chrono::steady_clock::time_point start, chrono::steady_clock::time_point end);