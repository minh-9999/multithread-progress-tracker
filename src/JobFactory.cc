#include "JobFactory.hh"
#include "JobBuilder.hh"
#include "log_utils.h"

// Create Job to initialize database
// - ID: "init-db"
// - Category: system
// - Priority: high (3)
// - Allow retry 1 time if failed
// - Timeout after 5s if not completed
// - When completed, log result + time
// - When error, log error details to stderr
Job createInitDatabaseJob()
{
    return JobBuilder()
        .withId("init-db")
        .withCategory("system")
        .withPriority(3)
        .withRetry(1)
        .withTimeout(5000)
        .withTask([]
                  {
                      //   cout << "\n [DB] Initializing database...\n";
                      SAFE_COUT("\n [DB] Initializing database...\n");
                      // simulateDBInit();
                  })
        .onComplete([](bool success, int attempt, long long ms)
                    { 
                        // cout << "\n [DB] Done (success = " << success << ",  took = " << ms << " ms)\n"; 
                        SAFE_COUT("\n [DB] Done (success = " << success << ",  took = " << ms << " ms)\n"); })
        .onError([](const string &err)
                 { 
                    // cerr << "\n [DB] Error: " << err << "\n";
                    SAFE_CERR("\n [DB] Error: " << err << "\n"); })
        .build();
}

// Create Job to generate periodic report
// - ID: "gen-report"
// - Category: analytics
// - Priority: medium (2)
// - No retry if failed
// - No time limit (be careful of hanging tasks!)
// - When finished, log execution time
Job createGenerateReportJob()
{
    return JobBuilder()
        .withId("gen-report")
        .withCategory("analytics")
        .withPriority(2)
        .withRetry(0)
        .withTask([]
                  {
                      //   cout << "\n [REPORT] Generating monthly report...\n";
                      SAFE_COUT("\n [REPORT] Generating monthly report...\n");
                      // generateReport();
                  })
        .onComplete([](bool ok, int attempt, long long ms)
                    { 
                        // cout << "\n [REPORT] Generated in " << ms << "ms\n";
                        SAFE_COUT("\n [REPORT] Generated in " << ms << "ms\n"); })
        .build();
}

// Create temporary garbage cleaning job in the system
// - ID: "cleanup-temp"
// - Category: maintenance
// - No retry set
// - Timeout after 2s → if running longer will be canceled
// - No callback when completed or error (can be added later)
Job createCleanupTempFilesJob()
{
    return JobBuilder()
        .withId("cleanup-temp")
        .withCategory("maintenance")
        .withTask([]
                  {
                      //   cout << "\n [CLEANUP] Removing temp files...\n";
                      SAFE_COUT("\n [CLEANUP] Removing temp files...\n");
                      // cleanupFiles();
                  })
        .withTimeout(2000)
        .build();
}

// Create Job to call API from outside
// - ID: "fetch-api"
// - Category: network
// - Allow retry up to 3 times if failed
// - Timeout after 3s
// - If timeout occurs, log warning
// - No callback when successful or normal error (can be added if more debugging is needed)
Job createFetchAPIDataJob()
{
    return JobBuilder()
        .withId("fetch-api")
        .withCategory("network")
        .withRetry(3)
        .withTimeout(3000)
        .withTask([]
                  {
                      //   cout << "\n [API] Fetching data...\n";
                      SAFE_COUT("\n [API] Fetching data...\n");
                      // fetchData();
                  })
        .onTimeout([]
                   { 
                        // cerr << "\n [API] Request timed out\n";
                        SAFE_CERR("\n [API] Request timed out\n"); })
        .build();
}
