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
}
