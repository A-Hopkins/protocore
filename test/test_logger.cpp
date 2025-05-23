#include "logger.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <thread>

// Helper: read all non-empty lines from a file
static std::vector<std::string> read_lines(const std::string& path)
{
  std::ifstream            ifs(path);
  std::vector<std::string> lines;
  if (!ifs.is_open())
  {
    return lines; // file not found or inaccessible
  }
  std::string line;
  while (std::getline(ifs, line))
  {
    if (!line.empty())
      lines.push_back(line);
  }
  return lines;
}

/**
 * @brief Test fixture for Logger and FileSink
 */
class LoggerTest : public ::testing::Test
{
protected:
  static void SetUpTestSuite()
  {
    // Prepare temporary log file and logger before any tests
    logPath = "test_logger.log";
    std::filesystem::remove(logPath);
    auto& lg = Logger::instance();
    lg.set_level(LogLevel::DEBUG).add_sink(std::make_unique<FileSink>(logPath));
  }

  static void TearDownTestSuite()
  {
    // Shutdown logger after all tests
    Logger::instance().shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::filesystem::remove(logPath);
  }

  void SetUp() override
  {
    // Clear file contents before each test
    std::ofstream ofs(logPath, std::ios::trunc);
    Logger::instance().set_level(LogLevel::DEBUG);
  }

  static std::string logPath;
};

// Definition of the static member
std::string LoggerTest::logPath;

TEST_F(LoggerTest, WritesEntriesToFile)
{
  auto& lg = Logger::instance();
  lg.log(LogLevel::INFO, "TEST", "Info entry");
  lg.log(LogLevel::DEBUG, "TEST", "Debug entry");
  lg.log(LogLevel::ERROR, "TEST", "Error entry");

  // Allow background thread to process
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  auto lines = read_lines(LoggerTest::logPath);
  ASSERT_EQ(lines.size(), 3u);
  EXPECT_NE(lines[0].find("Info entry"), std::string::npos);
  EXPECT_NE(lines[1].find("Debug entry"), std::string::npos);
  EXPECT_NE(lines[2].find("Error entry"), std::string::npos);
}

TEST_F(LoggerTest, FiltersBelowMinimumLevel)
{
  Logger::instance().set_level(LogLevel::WARN);
  auto& lg = Logger::instance();
  lg.log(LogLevel::INFO, "TEST", "Should be filtered out");
  lg.log(LogLevel::ERROR, "TEST", "Should appear");

  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  auto lines = read_lines(LoggerTest::logPath);
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_NE(lines[0].find("Should appear"), std::string::npos);
}

TEST_F(LoggerTest, MultipleEntriesMaintainOrder)
{
  auto& lg = Logger::instance();
  lg.log(LogLevel::DEBUG, "TST", "First");
  lg.log(LogLevel::DEBUG, "TST", "Second");
  lg.log(LogLevel::DEBUG, "TST", "Third");

  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  auto lines = read_lines(LoggerTest::logPath);
  ASSERT_GE(lines.size(), 3u);
  EXPECT_NE(lines[0].find("First"), std::string::npos);
  EXPECT_NE(lines[1].find("Second"), std::string::npos);
  EXPECT_NE(lines[2].find("Third"), std::string::npos);
}

TEST_F(LoggerTest, ConcurrentLogging)
{
  auto&     lg                = Logger::instance();
  const int threadCount       = 4;
  const int messagesPerThread = 50;

  auto worker = [&](int id)
  {
    for (int i = 0; i < messagesPerThread; ++i)
    {
      lg.log(LogLevel::DEBUG, "T" + std::to_string(id), "Msg" + std::to_string(i));
    }
  };

  std::vector<std::thread> threads;
  for (int t = 0; t < threadCount; ++t)
  {
    threads.emplace_back(worker, t);
  }
  for (auto& th : threads)
    th.join();

  // Allow flush
  std::this_thread::sleep_for(std::chrono::milliseconds(40));

  auto lines = read_lines(LoggerTest::logPath);
  // Expect total messages
  EXPECT_EQ(lines.size(), threadCount * messagesPerThread);
}
