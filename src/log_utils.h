

#pragma once
#include <mutex>
#include <iostream>

using namespace std;

inline mutex g_logMutex;

#define SAFE_COUT(x)                        \
    {                                       \
        lock_guard<mutex> lock(g_logMutex); \
        cout << "\n"                        \
             << x << endl;                  \
    }

#define SAFE_CERR(msg)                      \
    {                                       \
        lock_guard<mutex> lock(g_logMutex); \
        cerr << "\n"                        \
             << msg << flush;               \
    }