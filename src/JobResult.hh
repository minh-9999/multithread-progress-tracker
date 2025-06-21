#pragma once

#include <string>
#include <chrono>
#include <optional>

using namespace std;
using namespace chrono;

struct JobResult
{
    bool success = false;
    int attempts = 0;
    long long durationMs = 0;

    string jobId;                  // Unique ID of the job (if any)
    string category = "default";   // Job grouping
    optional<string> errorMessage; // Only available if failed

    system_clock::time_point startTime;
    system_clock::time_point endTime;

    string toJSON() const
    {
        string json = "{";
        json += "\"jobId\": \"" + jobId + "\", ";
        json += "\"category\": \"" + category + "\", ";
        json += "\"success\": " + string(success ? "true" : "false") + ", ";
        json += "\"attempts\": " + to_string(attempts) + ", ";
        json += "\"durationMs\": " + to_string(durationMs) + ", ";

        if (errorMessage.has_value())
            json += "\"error\": \"" + *errorMessage + "\", ";

        json += "\"timestamp\": \"" + to_string(system_clock::to_time_t(endTime)) + "\"";

        json += "}";
        return json;
    }
};
