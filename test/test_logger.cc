#include <gtest/gtest.h>
#include "../src/Logger.hh"

TEST(LoggerTest, WriteLogToFile)
{
    const string testLogFile = "logger_test_output.txt";
    Logger::init(testLogFile);
    Logger::log("Test message");

    ifstream in(testLogFile);
    ASSERT_TRUE(in.is_open());

    string content;
    getline(in, content);
    in.close();

    EXPECT_NE(content.find("Test message"), string::npos);
}

TEST(LoggerTest, DualSafeLogWritesBothPlaces)
{
    const string testLogFile = "logger_dual_test.txt";
    Logger::init(testLogFile);
    Logger::dualSafeLog("Dual test message");

    ifstream in(testLogFile);
    ASSERT_TRUE(in.is_open());

    string content;
    getline(in, content);
    in.close();

    EXPECT_NE(content.find("Dual test message"), string::npos);
}
