/**
 * @file task_state.h
 * @brief Defines the different states a task can be in and the Task class.
 *
 */
#pragma once

#include <string>

namespace task
{
  /**
   * @enum TaskState
   * @brief Represents the different states a task can be in.
   */
  enum class TaskState
  {
    NOT_STARTED, ///< Task has not yet started.
    IDLE,        ///< Task is idle, waiting for work.
    RUNNING,     ///< Task is currently executing.
    STOPPED,     ///< Task has been stopped.
    ERROR        ///< Task has encountered an error.
  };

  /**
   * @brief Returns string equivalent of the task state.
   *
   * @param state
   * @return std::string
   */
  static std::string task_state_to_string(TaskState state)
  {
    switch (state)
    {
      case TaskState::NOT_STARTED:
        return "NOT_STARTED";
      case TaskState::IDLE:
        return "IDLE";
      case TaskState::RUNNING:
        return "RUNNING";
      case TaskState::STOPPED:
        return "STOPPED";
      case TaskState::ERROR:
        return "ERROR";
      default:
        return "UNKNOWN";
    }
  }
} // namespace task
