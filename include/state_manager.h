/**
 * @file state_manager.h
 * @brief Defines the StateManager class, responsible for managing task states and transitions.
 *
 * The StateManager class extends Task and serves as a central controller that registers,
 * monitors, and coordinates state transitions for multiple tasks. It ensures synchronized
 * state changes across tasks and manages shutdown procedures. There should only be one
 * instance of StateManager in the system, and should be the main control point for all task states.
 */

#pragma once

#include <unordered_map>
#include <mutex>
#include <condition_variable>

#include "task.h"

const std::chrono::seconds STATE_TRANSITION_TIMEOUT = std::chrono::seconds(5); ///< Default timeout for state transitions.

/**
* @class StateManager
* @brief Manages state transitions and synchronization across multiple tasks.
*
* The StateManager is responsible for:
* - Registering tasks and tracking their states.
* - Coordinating state transitions based on requests.
* - Ensuring all tasks acknowledge state changes.
* - Managing system initialization and shutdown.
* 
* The StateManager keeps a record for tasks states to indicate if there is misalignment
* between the tasks and the state manager. In the case a task cannot respond but has set its internal state.
* States and tasks are tracked with a heap allocated unordered_map. Because it is tracking states and pointers
* that should be allocated at initialization of the system.
*/
class StateManager : public task::Task
{
public:
  /**
  * @brief Constructs a StateManager instance and initializes state handling.
  */
  StateManager();

  /**
  * @brief Registers a task with the state manager and sets its initial state to IDLE.
  * @param task Pointer to the task being registered.
  */
  void register_task(task::Task* task);

  /**
  * @brief Requests a transition to a new state for all registered tasks.
  * @param new_state The state to transition to.
  */
  void request_state_transition(task::TaskState new_state);

  /**
  * @brief Demands a state transition for all registered tasks and waits for acknowledgment.
  * 
  * @param new_state The state to transition to.
  * @param timeout The maximum time to wait for acknowledgment.
  * @return True if all tasks have transitioned to the new state, false otherwise.
  */
  bool demand_state_transition(task::TaskState new_state, std::chrono::seconds timeout = STATE_TRANSITION_TIMEOUT);

  /**
  * @brief Initializes the state manager and starts all registered tasks.
  *
  * This function transitions all tasks to the `IDLE` state and begins execution.
  */
  void initialize();

  /**
  * @brief Shuts down all registered tasks and ensures they stop gracefully.
  *
  * This function requests all tasks to transition to the `STOPPED` state and waits
  * for acknowledgment before stopping them.
  */
  void shutdown();

protected:
  /**
  * @brief Handles transitions between different task states.
  * @param new_state The state to transition to.
  */
  void transition_to_state(task::TaskState new_state);

  /**
  * @brief Processes messages received by the state manager.
  * @param msg The message to be processed.
  */
  void process_message(const msg::Msg& msg);

private:
  std::unordered_map<task::Task*, task::TaskState> task_states; ///< Tracks the current state of each registered task.
  std::mutex state_mutex; ///< Mutex for synchronizing state transitions.
  std::condition_variable shutdown_cv; ///< Condition variable used to coordinate shutdown.
  std::condition_variable state_transition_cv; ///< Condition variable used to coordinate state transitions.
  task::TaskState current_state; ///< The current state of the state manager.
  task::TaskState target_state; ///< The target state for a state transition.

  /**
  * @brief Handles acknowledgment messages from tasks confirming state transitions.
  * @param msg The acknowledgment message received from a task.
  */
  void handle_acknowledgment(const msg::Msg& msg);

  /**
  * @brief Checks if all registered tasks have reached a specific state.
  * 
  * @param state The state to check for
  * @return True if all tasks are in the specified state
  */
  bool all_tasks_in_state(task::TaskState state) const;
};
