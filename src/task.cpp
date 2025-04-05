/**
 * @file task.cpp
 * @brief Implementtion of the Task class, which serves as a base class for task execution and state management.
 *
 * This implementation enforces that tasks are managed by shared pointers (via std::enable_shared_from_this)
 * and includes safe subscribe/publish functions with additional runtime checks.
 */
#include <iostream>
#include <chrono>
#include <cassert>

#include "broker.h"
#include "task.h"

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
      running = true;
      queue_thread = std::thread(&Task::run, this);

      // Start the periodic task thread if the interval is set.
      if (periodic_task_interval_ms > 0)
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
    std::cout << "Task " << name << " transitioned to state: " << task_state_to_string(current_state) << std::endl;
    safe_publish(msg::Msg(this, msg::StateAckMsg{static_cast<uint8_t>(current_state)}));
  }
  
  void Task::run_periodic()
  {
    // Wait until the next periodic execution time.
    auto next_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(periodic_task_interval_ms);
    std::this_thread::sleep_until(next_time);

    // Execute periodic processing.
    periodic_task_process();
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
}
