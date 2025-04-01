/**
 * @file task.cpp
 * @brief Implementtion of the Task class, which serves as a base class for task execution and state management.
 *
 */
#include <iostream>
#include <chrono>

#include "task.h"

namespace task
{
  task::Task::~Task()
  {
    stop();
    if (task_thread.joinable())
    {
      task_thread.join();
    }
  }
  
  void task::Task::start()
  {
    if (!running)
    {
      running = true;
      transition_to_state(TaskState::IDLE);
      task_thread = std::thread(&Task::run, this);
    }
  }
  
  void task::Task::stop()
  {
    if (running)
    {
      running = false;
      transition_to_state(TaskState::STOPPED);
      if (task_thread.joinable())
      {
        task_thread.join();
      }
    }
  }
  
  void task::Task::transition_to_state(TaskState new_state)
  {
    current_state = new_state;
    std::cout << "Task " << name << " transitioned to state: " << task_state_to_string(current_state) << std::endl;
  }
  
  void task::Task::run_periodic()
  {
    // Wait until the next periodic execution time.
    auto next_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(periodic_task_interval_ms);
    std::this_thread::sleep_until(next_time);

    // Immediately try to dequeue any pending message (non-blocking).
    auto opt_msg = message_queue.try_dequeue(std::chrono::milliseconds(0));
    if (opt_msg)
    {
        process_message(*opt_msg);
    }

    // Execute periodic processing.
    periodic_task_process();
  }

  void task::Task::run_blocking()
  {
    msg::Msg msg = message_queue.dequeue();
    process_message(msg);
  }

  void task::Task::run()
  {

    while (running)
    {
      if (periodic_task_interval_ms > 0)
      {
        run_periodic();
      }
      else
      {
        run_blocking();
      }
    }
  }
}
