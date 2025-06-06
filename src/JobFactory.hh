#pragma once
#include "Job.hh"

Job createInitDatabaseJob();

Job createGenerateReportJob();

Job createCleanupTempFilesJob();

Job createFetchAPIDataJob();
