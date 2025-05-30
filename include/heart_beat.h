/**
 * @file heart_beat.h
 * @brief Heart beat monitoring functionality for system health tracking
 *
 * This file defines the HeartBeatTask class which implements a periodic heartbeat
 * mechanism to monitor the health of system tasks. It sends heartbeat messages to
 * registered tasks and tracks their responses, taking action when tasks become unresponsive.
 */
#pragma once

#include "task/task.h"
#include <map>

class StateManager; ///< Forward declaration of StateManager class.

/**
 * @class HeartbeatTask
 * @brief Task responsible for monitoring system health through periodic heartbeats
 *
 * This task sends periodic heartbeat messages to registered tasks and monitors
 * their acknowledgments. If a task fails to respond within the timeout period,
 * it can notify the StateManager to take appropriate action.
 */
class HeartBeatTask : public task::Task
{
public:
  /**
   * @brief Factory method for creating a HeartbeatTask instance.
   *
   * @param task_name Name of the task
   * @param state_manager Pointer to the StateManager
   * @param heartbeat_interval_ms Interval between heartbeats in milliseconds
   * @param response_timeout_ms Time to wait for acknowledgment in milliseconds
   * @return std::shared_ptr<HeartbeatTask> Shared pointer to new HeartbeatTask
   */
  static std::shared_ptr<HeartBeatTask>
  create(const std::string& name, std::shared_ptr<StateManager> state_manager,
         std::chrono::milliseconds heartbeat_interval_ms = std::chrono::milliseconds(10000),
         std::chrono::milliseconds response_timeout_ms   = std::chrono::milliseconds(3000))
  {
    auto instance = std::shared_ptr<HeartBeatTask>(
        new HeartBeatTask(name, state_manager, heartbeat_interval_ms, response_timeout_ms));
    instance->on_initialize();
    return instance;
  }

  /**
   * @brief Registers a task to be monitored via heartbeats
   *
   * @param task The task to monitor
   */
  void notify_task_registered(std::shared_ptr<task::Task> task);

  /**
   * @brief Sets the action to take when a task becomes unresponsive
   *
   * @param action Action to take (ERROR, RESET, etc.)
   */
  void set_unresponsive_action(task::TaskState action) { unresponsive_action = action; }

protected:
  /**
   * @brief Constructor for HeartBeatTask
   *
   * @param name Name of the task
   * @param state_manager Pointer to the StateManager
   * @param heartbeat_interval_ms Interval between heartbeats in milliseconds
   * @param response_timeout_ms Time to wait for acknowledgment in milliseconds
   */
  HeartBeatTask(const std::string&            name              = "HeartBeat",
                std::shared_ptr<StateManager> state_manager     = nullptr,
                std::chrono::milliseconds heartbeat_interval_ms = std::chrono::milliseconds(10000),
                std::chrono::milliseconds response_timeout_ms   = std::chrono::milliseconds(3000))
    : task::Task(name), state_manager(state_manager), response_timeout_ms(response_timeout_ms)
  {
    set_periodic_task_interval(heartbeat_interval_ms);
  }

  /**
   * @brief Initialize task subscriptions and observers
   */
  void on_initialize() override;

  /**
   * @brief Process incoming messages including heartbeat acknowledgments
   *
   * @param msg The message to process
   */
  void process_message(const msg::Msg& msg) override;

  /**
   * @brief Perform periodic heartbeat operations including sending heartbeats and checking timeouts
   */
  void periodic_task_process() override;

private:
  /**
   * @struct TaskStatus
   * @brief Tracks the heartbeat status for a monitored task
   *
   * Contains information about the last heartbeat sent to a task and
   * whether an acknowledgment is pending.
   */
  struct TaskStatus
  {
    bool     awaiting_response = false; ///< Whether a heartbeat response is pending
    uint32_t last_heartbeat_id = 0;     ///< ID of the last heartbeat sent
    std::chrono::steady_clock::time_point last_heartbeat_time; ///< Time the last heartbeat was sent
    std::chrono::steady_clock::time_point
        last_response_time; ///< Time the last response was received
  };

  std::map<std::shared_ptr<task::Task>, TaskStatus>
                                monitored_tasks;     ///< Map of monitored tasks and their status
  std::mutex                    tasks_mutex;         ///< Mutex to protect access to monitored_tasks
  std::shared_ptr<StateManager> state_manager;       ///< Pointer to the state manager
  std::chrono::milliseconds     response_timeout_ms; ///< Timeout for heartbeat responses
  std::atomic<uint32_t>         next_heartbeat_id{1}; ///< Counter for generating heartbeat IDs
  task::TaskState               unresponsive_action =
      task::TaskState::ERROR; ///< Action to take when a task is unresponsive

  /**
   * @brief Generate a unique heartbeat ID
   *
   * @return uint32_t The generated ID
   */
  uint32_t generate_heartbeat_id();

  /**
   * @brief Check for tasks that have timed out responding to heartbeats
   */
  void check_for_timeouts();

  /**
   * @brief Send heartbeats to monitored tasks
   */
  void send_heartbeat();

  /**
   * @brief Process heartbeat acknowledgment messages
   *
   * @param msg The acknowledgment message
   */
  void handle_acknowledgment(const msg::Msg& msg);

  /**
   * @brief Handle a task that has failed to respond to heartbeats
   *
   * @param task The unresponsive task
   */
  void handle_unresponsive_task(std::shared_ptr<task::Task> task);
};
