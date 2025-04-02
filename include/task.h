/**
 * @file task.h
 * @brief Defines the Task class, which serves as a base class for task execution and state management.
 *
 * The Task class provides a framework for managing tasks with different states (IDLE, RUNNING, STOPPED, ERROR).
 * It includes functionality for state transitions and thread management.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include <string>

#include "message_queue.h"

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

  /**
   * @class Task
   * @brief A base class that represents a task with a state machine and message-based communication.
   * 
   * This class provides a framework for creating tasks that can run in their own threads,
   * process messages from a message queue, and manage their own state transitions.
   * Derived classes must implement the process_message() method to handle incoming messages.
   * The task can be started and stopped, and it can also perform periodic tasks based on a specified interval.
   * The task's state can be queried, and the task can be transitioned to different states.
   * A task can perform periodic processing if the periodic_task_interval_ms is set to a non-zero value.
   * The task will run two threads a timer for processing periodically and a thread for processing messages exclusively.
   */
  class Task
  {
  public:
    MessageQueue message_queue; ///< The message queue for storing incoming messages.

    /**
     * @brief Constructor for the Task class.
     * @param task_name The name of the task.
     * @param queue_msg_count The maximum number of messages in the queue. Default is 64.
     * 
     * A task will run in its own thread and can process messages from the queue. The queue is created
     * with a size of message that is the node size used for the memory pool and some number of messages
     * to be stored in the queue.
     * The task will be in the NOT_STARTED state until start() is called.
     * The task will be in the IDLE state when it is not processing messages. The process_message() method
     * must be implemented by derived classes to handle incoming messages, to enforce its own state machine.
     */
    Task(const std::string& task_name = "UnnamedTask", const std::size_t queue_msg_count = 64)
      : name(task_name),
        current_state(TaskState::NOT_STARTED),
        running(false),
        message_queue(MessageQueue::node_size(), queue_msg_count)
    { }

    /**
     * @brief Virtual destructor to ensure proper cleanup in derived classes.
     */
    virtual ~Task();

    /**
     * @brief Starts the task execution.
     */
    void start();

    /**
     * @brief Stops the task execution.
     */
    void stop();

    /**
     * @brief Checks if the task is currently running.
     * @return True if the task is running, otherwise false.
     */
    bool is_running() const { return running; }

    /**
     * @brief Gets the name of the task
     * @return The name of the task
     */
    const std::string& get_name() const { return name; }
    
    /**
     * @brief Gets the current state of the task.
     * @return The current state of the task.
     */
    TaskState get_current_state() const { return current_state; }

  protected:
    std::string name; ///< The name of the task.
    TaskState current_state; ///< The current state of the task.


    /**
     * @brief Sets the periodic task interval.
     * @param interval_ms The interval in milliseconds.
     */
    void set_periodic_task_interval(unsigned int interval_ms) { periodic_task_interval_ms = interval_ms; }

    /**
     * @brief Virtual function to be overridden by derived classes for periodic task processing.
     *
     * This function is called periodically based on the set interval and will not get called
     * if the interval is set to 0. Derived classes should implement this function to define
     * if they need periodic behavior of the task.
     */
    virtual void periodic_task_process() { }

    /**
     * @brief Virtual function to be overridden by derived classes for message processing.
     *
     * Derived classes must implement this function to define how incoming messages
     * should be processed. This function is called whenever a new message is retrieved
     * from the message queue.
     *
     * @param msg The message to process. The `msg::Msg` parameter contains the data
     *            and metadata associated with the message. Derived classes should
     *            ensure proper handling of the message content and any associated
     *            constraints, such as thread safety or message validity.
     *
     * @note This function must not block indefinitely, as it may interfere with
     *       the task's ability to process other messages or perform periodic tasks.
     */
    virtual void process_message(const msg::Msg& msg) = 0;

    /**
     * @brief Transitions the task to a new state.
     *
     * This method changes the current state of the task to the specified `new_state`.
     * It validates the state transition to ensure it adheres to the task's state machine rules.
     * Derived classes can override this method to implement additional logic or side effects
     * during state transitions, such as logging, resource cleanup, or initialization.
     *
     * @param new_state The state to transition to.
     *
     * @note Invalid state transitions may result in an exception or error state.
     *       Ensure that the task is in a valid state before calling this method.
     */
    virtual void transition_to_state(TaskState new_state);

  private:
    std::atomic<bool> running; ///< Flag indicating if the task is running.
    std::thread queue_thread; ///< The thread in which the task runs and processes messages.
    std::thread periodic_thread; ///< The thread for periodic task execution. Only starts if periodic_task_interval_ms is set.
    uint16_t periodic_task_interval_ms = 0; ///< The interval for periodic tasks in milliseconds.

    /**
     * @brief The main execution loop for processing messages
     */
    void run();

    /**
     * @brief The execution loop for periodic tasks. Does not run if periodic_task_interval_ms is 0.
     */
    void run_periodic();

  };
}