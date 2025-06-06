#include <gtest/gtest.h>
#include "../src/Logger.hh"

TEST(LoggerTest, WriteLogToFile)
{
    const string testLogFile = "logger_test_output.txt";
    // Logger::init(testLogFile);
    Logger &logger = Logger::instance();
    logger.start(testLogFile, true);
    Logger::log(LogLevel::Info, "LoggerTest", "OK", 123, 1);

    ifstream in(testLogFile);
    ASSERT_TRUE(in.is_open());

    string content;
    getline(in, content);
    in.close();

    EXPECT_NE(content.find("LoggerTest"), string::npos);
    EXPECT_NE(content.find("OK"), string::npos);
    EXPECT_NE(content.find("123"), string::npos);
}

TEST(LoggerTest, DualSafeLogWritesBothPlaces)
{
    const string testLogFile = "logger_dual_test.txt";
    // Logger::init(testLogFile);
    Logger &logger = Logger::instance();
    logger.start(testLogFile, true);
    Logger::dualSafeLog("Dual test message");

    ifstream in(testLogFile);
    ASSERT_TRUE(in.is_open());

    string content;
    getline(in, content);
    in.close();

    EXPECT_NE(content.find("Dual test message"), string::npos);
}
