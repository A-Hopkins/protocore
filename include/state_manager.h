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

#include <map>
#include <mutex>
#include <condition_variable>

#include "task.h"

const std::chrono::milliseconds STATE_TRANSITION_TIMEOUT = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds(5)); ///< Default timeout for state transitions.

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
   * @brief Factory method for creating a StateManager instance.
   *
   * This function constructs and initializes a new StateManager object.
   * It ensures that the instance is properly managed by a shared pointer and fully
   * initialized via the on_initialize() hook. The StateManager's create method is an
   * example of how derived classes should implement their own factory methods.
   *
   * @return std::shared_ptr<StateManager> A shared pointer to the newly created StateManager.
   *
   * @note Derived classes should follow a similar pattern:
   * - Make the constructor protected.
   * - Provide a public static create() method.
   * - Call on_initialize() after the object is managed by a shared pointer.
   *
   * @code
   * // Example for a derived task:
   * class MyTask : public task::Task {
   * public:
   *     static std::shared_ptr<MyTask> create(const std::string &name) 
   *     {
   *         return std::shared_ptr<MyTask>(new MyTask(name));
   *     }
   * protected:
   *     MyTask(const std::string &name) : task::Task(name) { }
   *     void on_initialize() override
   *     {
   *         safe_subscribe(msg::Type::MyCustomMsg);
   *     }
   *     void process_message(const msg::Msg &msg) override { // Task-specific message processing... }
   * };
   * @endcode
   */
  static std::shared_ptr<StateManager> create()
  {
    auto instance = std::shared_ptr<StateManager>(new StateManager("StateManager"));
    instance->on_initialize();
    return instance;
  }

  /**
  * @brief Registers a task with the state manager and sets its initial state to IDLE.
  * @param task Pointer to the task being registered.
  */
 void register_task(const std::shared_ptr<task::Task>& task);

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
  bool demand_state_transition(task::TaskState new_state, std::chrono::milliseconds timeout = STATE_TRANSITION_TIMEOUT);

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

  /**
   * @brief Constructor for the StateManager class.
   *
   * This constructor initializes the state manager with default values and sets
   * the current state to NOT_STARTED. The constructor is protected to enforce the
   * use of the create() factory method for instantiation.
   */
  StateManager(const std::string &name = "StateManager") : Task(name) { }

  /**
   * @brief Performs initialization tasks for the StateManager.
   *
   * This virtual function is called during the creation process (via Task::create) to perform any necessary
   * initialization, such as subscribing to the appropriate message types. The default implementation subscribes
   * the StateManager to the StateAckMsg message type. Derived classes may override this function to perform
   * additional initialization tasks.
   */
  virtual void on_initialize() override
  {
    safe_subscribe(msg::Type::StateAckMsg);
  }

private:
  std::map<std::shared_ptr<task::Task>, task::TaskState> task_states; ///< Tracks the current state of each registered task.
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
