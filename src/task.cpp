/**
 * @file task.cpp
 * @brief Implementtion of the Task class, which serves as a base class for task execution and state
 * management.
 *
 * This implementation enforces that tasks are managed by shared pointers (via
 * std::enable_shared_from_this) and includes safe subscribe/publish functions with additional
 * runtime checks.
 */
#include "task/task.h"
#include "broker.h"
#include <cassert>
#include <chrono>
#include <iostream>

namespace task
{
  Task::~Task()
  {
    stop();
  }

  void Task::start()
  {
    if (!running)
    {
      running      = true;
      queue_thread = std::thread(&Task::run, this);

      // Start the periodic task thread if the interval is set.
      if (periodic_task_interval_ms > std::chrono::milliseconds(0))
      {
        periodic_thread = std::thread(&Task::run_periodic, this);
      }
    }
  }

  void Task::stop()
  {
    if (running)
    {
      // First set running to false then notify shutdown.
      running = false;
      transition_to_state(TaskState::STOPPED);

      message_queue.notify_shutdown();

      if (queue_thread.joinable())
      {
        queue_thread.join();
      }
      if (periodic_thread.joinable())
      {
        periodic_thread.join();
      }
    }
  }

  void Task::transition_to_state(TaskState new_state)
  {
    current_state = new_state;
    Logger::instance().log(LogLevel::INFO, this->get_name(),
                           "transitioning to " + task_state_to_string(new_state));
    safe_publish(msg::Msg(this, msg::StateAckMsg{static_cast<uint8_t>(current_state)}));
  }

  void Task::run_periodic()
  {
    while (running)
    {
      // Execute periodic processing.
      periodic_task_process();

      // Wait until the next periodic execution time.
      auto next_time = std::chrono::steady_clock::now() + periodic_task_interval_ms;
      std::this_thread::sleep_until(next_time);
    }
  }

  void Task::run()
  {
    while (running)
    {
      auto opt = message_queue.dequeue();
      if (opt)
      {
        process_message(*opt);
      }
    }
  }

  void Task::safe_subscribe(msg::Type type)
  {
    std::shared_ptr<Task> self = shared_from_this();
    assert(self && "Task must be created using Task::create() to use safe_subscribe.");
    Broker::subscribe(self, type);
  }

  void Task::safe_publish(const msg::Msg& msg)
  {
    std::shared_ptr<Task> self = shared_from_this();
    assert(self && "Task must be created using Task::create() to use safe_publish.");
    Broker::publish(msg);
  }

  void Task::handle_heartbeat(const msg::HeartbeatMsg* heartbeat_msg)
  {
    msg::HeartbeatAckMsg ack;
    ack.orig_unique_id = heartbeat_msg->unique_id;
    ack.orig_timestamp = heartbeat_msg->timestamp;
    ack.ack_timestamp  = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch())
                            .count();
    safe_publish(msg::Msg(this, ack));
  }
} // namespace task
