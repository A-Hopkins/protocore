/**
 * @file logger.cpp
 * @brief Implementation fo logger class and sinks
 */

#include "logger.h"
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

static const std::chrono::steady_clock::time_point STARTUP_TIME = std::chrono::steady_clock::now();

/**
 * @brief Constructs a FileSink that writes log entries to the specified file.
 *
 * Attempts to open the file in append mode. If the file does not exist, it
 * is created. Throws on failure to create or open.
 *
 * @param path Filesystem path to the log file.
 * @throws std::runtime_error if the file cannot be created or opened.
 */
FileSink::FileSink(const std::string& path) : ofs(path, std::ios::out | std::ios::app)
{
  if (!ofs)
  {
    // If initial open fails, attempt explicit creation
    std::ofstream createFile(path);

    if (!createFile)
    {
      throw std::runtime_error("FileSink: Unable to create log file: " + path);
    }

    createFile.close();

    // Re-open for appending
    ofs.open(path, std::ios::out | std::ios::app);
    if (!ofs)
    {
      throw std::runtime_error("FileSink: Unable to open log file after creation: " + path);
    }
  }
}

/**
 * @brief Destructor for FileSink.
 *
 * Closes the underlying file stream automatically.
 */
FileSink::~FileSink() = default;

/**
 * @brief Writes a formatted log line to the file.
 *
 * Acquires a lock to ensure thread-safe access.
 *
 * @param formatted Pre-formatted log line to write.
 */
void FileSink::log(const std::string& formatted)
{
  std::lock_guard<std::mutex> lock(ofs_mutex);
  ofs << formatted << '\n';
  ofs.flush();
}

// SocketSink is left unimplemented for now
SocketSink::SocketSink(const std::string& host, uint16_t port) : sockfd(-1), connected(false)
{
  // TODO: Implement socket connection
}

SocketSink::~SocketSink()
{
  if (sockfd >= 0)
  {
    ::shutdown(sockfd, SHUT_RDWR);
    ::close(sockfd);
  }
}

bool SocketSink::connectWithRetry(const std::string& host, uint16_t port, int retryCount,
                                  std::chrono::milliseconds retryDelay)
{
  // TODO: Implement retry logic
  return false;
}

void SocketSink::log(const std::string& formatted)
{
  // TODO: Implement socket logging
}

/**
 * @brief Retrieves the singleton Logger instance.
 *
 * @return Reference to the global Logger object.
 */
Logger& Logger::instance()
{
  static Logger inst;
  return inst;
}

/**
 * @brief Constructs the Logger and starts the background dispatch thread.
 *
 * Initializes the minimum log level and spawns the worker thread.
 */
Logger::Logger() : min_level(LogLevel::MSG_TRAFFIC), stopping(false)
{
  worker = std::thread(&Logger::worker_loop, this);
}

/**
 * @brief Destructor for Logger.
 *
 * Signals shutdown and joins the worker thread, ensuring all entries
 * are flushed before exit.
 */
Logger::~Logger()
{
  shutdown();
  if (worker.joinable())
  {
    worker.join();
  }
}

/**
 * @brief Registers a new output sink.
 *
 * Adds the provided ILogSink to the internal list. Entries
 * will be dispatched to this sink as they are processed.
 *
 * @param sink Unique pointer to an ILogSink implementation.
 * @return Reference to this Logger for chaining.
 */
Logger& Logger::add_sink(std::unique_ptr<ILogSink> sink)
{
  sinks.emplace_back(std::move(sink));
  return *this;
}

/**
 * @brief Sets the minimum severity level for logging.
 *
 * Log entries with a level lower than min_level are discarded.
 *
 * @param level Minimum LogLevel to accept.
 * @return Reference to this Logger for chaining.
 */
Logger& Logger::set_level(LogLevel level)
{
  min_level.store(level);
  return *this;
}

Logger& Logger::use_relative_timestamps(bool enable)
{
  relative_ts.store(enable);
  return *this;
}

/**
 * @brief Queues a log entry if its level meets the threshold.
 *
 * Captures a monotonic timestamp and enqueues the LogEntry
 * for asynchronous processing by the worker thread.
 *
 * @param level Severity level of the log entry.
 * @param component Name or identifier of the calling context.
 * @param message Human-readable log message content.
 */
void Logger::log(LogLevel level, const std::string& component, const std::string& message)
{
  if (level < min_level.load())
  {
    return;
  }

  uint64_t ms;

  if (relative_ts.load())
  {
    // Relative to logger startup
    auto now = std::chrono::steady_clock::now();
    ms       = std::chrono::duration_cast<std::chrono::milliseconds>(now - STARTUP_TIME).count();
  }
  else
  {
    auto now = std::chrono::system_clock::now();
    ms       = std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
             .count();
  }

  LogEntry entry{level, component, message, static_cast<uint64_t>(ms), std::this_thread::get_id()};
  {
    std::lock_guard<std::mutex> lock(queue_mutex);
    queue.emplace_back(std::move(entry));
  }
  queue_cv.notify_one();
}

/**
 * @brief Signals the worker thread to stop after flushing queued entries.
 *
 * Wakes up the worker thread to allow clean shutdown.
 */
void Logger::shutdown()
{
  {
    std::lock_guard<std::mutex> lock(queue_mutex);
    stopping = true;
  }
  queue_cv.notify_all();
}

/**
 * @brief Background worker function that processes queued log entries.
 *
 * Waits for entries to appear in the queue or for shutdown. Formats each
 * entry (timestamp, component, level, message) into a static buffer and
 * dispatches it to all registered sinks. Exits when stopping is true and
 * the queue is empty.
 */
void Logger::worker_loop()
{
  while (true)
  {
    LogEntry entry;
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      queue_cv.wait(lock, [this]() { return stopping || !queue.empty(); });
      if (stopping && queue.empty())
      {
        break;
      }
      entry = std::move(queue.front());
      queue.pop_front();
    }

    const char* level_str = nullptr;
    switch (entry.level)
    {
      case LogLevel::MSG_TRAFFIC:
        level_str = "TRAFFIC";
        break;
      case LogLevel::DEBUG:
        level_str = "DEBUG";
        break;
      case LogLevel::INFO:
        level_str = "INFO";
        break;
      case LogLevel::WARN:
        level_str = "WARN";
        break;
      case LogLevel::ERROR:
        level_str = "ERROR";
        break;
    }

    char buffer[512];
    int  len = std::snprintf(buffer, sizeof(buffer), "%llu [%s] %s - %s",
                             static_cast<unsigned long long>(entry.timestampMs),
                             entry.component.c_str(), level_str, entry.message.c_str());

    if (len < 0)
      len = 0;

    std::string formatted(buffer, static_cast<size_t>(len));

    // Dispatch to all sinks
    for (auto& sink : sinks)
    {
      sink->log(formatted);
    }
  }
}
