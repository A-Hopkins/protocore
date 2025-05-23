/**
 * @file logger.h
 * @brief Asynchronous, thread-safe logging framework for ProtoCore components.
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/**
 * @enum LogLevel
 * @brief Severity levels for logging.
 */
enum class LogLevel
{
  MSG_TRAFFIC, ///< Message traffic (broker publish)
  DEBUG,       ///< Detailed diagnostic information
  INFO,        ///< Routine information
  WARN,        ///< Indication of potential issues
  ERROR        ///< Errors that require attention
};

/**
 * @struct LogEntry
 * @brief Single log record containing metadata and message.
 */
struct LogEntry
{
  LogLevel        level;       ///< Severity level
  std::string     component;   ///< Originating component name
  std::string     message;     ///< Log message text
  uint64_t        timestampMs; ///< Epoch time in milliseconds
  std::thread::id threadId;    ///< ID of the logging thread
};

/**
 * @interface ILogSink
 * @brief Abstract interface for log output destinations.
 */
struct ILogSink
{
  virtual ~ILogSink() = default;

  /**
   * @brief Emit a formatted log string to the sink.
   * @param formatted Fully formatted log line.
   */
  virtual void log(const std::string& formatted) = 0;
};

/**
 * @class FileSink
 * @brief Writes log entries to a file (append mode).
 */
class FileSink : public ILogSink
{
public:
  /**
   * @brief Construct a FileSink.
   * @param path Filesystem path to the log file.
   */
  explicit FileSink(const std::string& path);
  ~FileSink() override;
  void log(const std::string& formatted) override;

private:
  std::ofstream ofs;       ///< Output file stream
  std::mutex    ofs_mutex; ///< Guards access to the file stream
};

/**
 * @class ConsoleSink
 * @brief Writes log entries to stdout
 */
class ConsoleSink : public ILogSink
{
public:
  explicit ConsoleSink(std::ostream& out = std::cout);
  ~ConsoleSink() override;
  void log(const std::string& formatted) override;

private:
  std::ostream& std_out;
  std::mutex    std_out_mutex;
};

// TODO: Implement socket writes
/**
 * @class SocketSink
 * @brief Streams log entries over a TCP socket.
 */
class SocketSink : public ILogSink
{
public:
  /**
   * @brief Construct a SocketSink.
   * @param host Remote server address (IPv4 string).
   * @param port TCP port number.
   */
  SocketSink(const std::string& host, uint16_t port);
  ~SocketSink();
  void log(const std::string& formatted) override;

private:
  /**
   * @brief Attempt to connect with retry logic.
   * @return true if connection succeeds.
   */
  bool connectWithRetry(const std::string& host, uint16_t port, int retryCount,
                        std::chrono::milliseconds retryDelay);

  int        sockfd;     ///< Socket file descriptor
  bool       connected;  ///< Connection state
  std::mutex sock_mutex; ///< Guards socket send operations
};

/**
 * @class Logger
 * @brief Central asynchronous logger with multiple sinks.
 *
 * Usage:
 *   Logger::instance()
 *     .setLevel(LogLevel::INFO)
 *     .addSink(std::make_unique<FileSink>("app.log"));
 *   Logger::instance().log(LogLevel::DEBUG, "MyComp", "Hello");
 */
class Logger
{
public:
  /**
   * @brief Retrieve singleton instance.
   */
  static Logger& instance();

  Logger(const Logger&)            = delete;
  Logger& operator=(const Logger&) = delete;
  Logger(Logger&&)                 = delete;
  Logger& operator=(Logger&&)      = delete;

  /**
   * @brief Register a new output sink.
   * @param sink Unique pointer to a sink implementation.
   */
  Logger& add_sink(std::unique_ptr<ILogSink> sink);

  /**
   * @brief Set global minimum severity level.
   * @param level Logs below this level will be discarded.
   */
  Logger& set_level(LogLevel level);

  /**
   * @brief Set useage of relative time stamps
   * @param enable true or false to enable relative to start up timestamps
   */
  Logger& use_relative_timestamps(bool enable);

  /**
   * @brief Emit a log entry asynchronously.
   * @param level Severity level.
   * @param component Component name tag.
   * @param message Human-readable message.
   */
  void log(LogLevel level, const std::string& component, const std::string& message);

  /**
   * @brief Gracefully stop the logging thread (flush pending entries).
   */
  void shutdown();

private:
  Logger();
  ~Logger();

  void worker_loop();

  std::atomic<LogLevel>                  min_level{LogLevel::MSG_TRAFFIC}; ///< Minimum log level
  std::atomic<bool>                      relative_ts;                      ///< Relative timestamp
  std::deque<LogEntry>                   queue;                            ///< Pending entries
  std::vector<std::unique_ptr<ILogSink>> sinks;                            ///< Registered sinks
  std::mutex                             queue_mutex;                      ///< Guards queue
  std::condition_variable                queue_cv;        ///< Notifies worker thread
  std::thread                            worker;          ///< Background dispatch thread
  bool                                   stopping{false}; ///< Shutdown flag
};