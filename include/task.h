/**
 * @file task.h
 * @brief Defines the Task class used as a base for task execution and state management.
 *
 * @section Usage
 * All tasks must be instantiated through a public static factory method implemented
 * in the derived class. The Task constructor is protected to enforce proper initialization
 * and to ensure that tasks are managed by shared pointers. Do not create a raw Task.
 * Instead, each derived class must provide its own create() method, for example:
 *
 * @code
 * class MyTask : public task::Task {
 * public:
 *     static std::shared_ptr<MyTask> create(const std::string &name) {
 *         return std::shared_ptr<MyTask>(new MyTask(name));
 *     }
 * protected:
 *     MyTask(const std::string &name) : Task(name) { }
 *     void on_initialize() override { safe_subscribe(msg::Type::MyMsg); }
 *     void process_message(const msg::Msg &msg) override {  }
 * };
 * @endcode
 */

#pragma once

#include "message_queue.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

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
   * @brief A base class representing a task with a state machine and message-based communication.
   *
   * This class enforces that tasks must be created via a static factory method in derived classes.
   * The Task constructor is protected to prevent raw instantiation; every derived task must
   * implement a public static create() method that calls the constructor and then calls
   * on_initialize() to perform any necessary setup (such as subscriptions). This design guarantees
   * that shared_from_this() is safe to use.
   */
  class Task : public std::enable_shared_from_this<Task>
  {
  public:
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

    /**
     * @brief Safely subscribes the task to a message type.
     *
     * This method wraps the Broker::subscribe call using the task's shared pointer.
     *
     * @param type The message type to subscribe to.
     */
    void safe_subscribe(msg::Type type);

    /**
     * @brief Safely publishes a message.
     *
     * This method wraps the Broker::publish call.
     *
     * @param msg The message to publish.
     */
    void safe_publish(const msg::Msg& msg);

    /**
     * @brief Delivers a message to this task.
     *
     * This method is used by the Broker to enqueue messages into the task's private message queue.
     *
     * @param msg The message to deliver.
     */
    void deliver_message(const msg::Msg& msg) { message_queue.enqueue(msg); }

  protected:
    std::string name;          ///< The name of the task.
    TaskState   current_state; ///< The current state of the task.

    /**
     * @brief Constructor for the Task class.
     * @param task_name The name of the task.
     * @param queue_msg_count The maximum number of messages in the queue. Default is 64.
     *
     * A task will run in its own thread and can process messages from the queue. The queue is
     * created with a size of message that is the node size used for the memory pool and some number
     * of messages to be stored in the queue. The task will be in the NOT_STARTED state until
     * start() is called. The task will be in the IDLE state when it is not processing messages. The
     * process_message() method must be implemented by derived classes to handle incoming messages,
     * to enforce its own state machine.
     */
    Task(const std::string& task_name = "UnnamedTask", const std::size_t queue_msg_count = 64)
      : name(task_name), current_state(TaskState::NOT_STARTED), running(false),
        message_queue(MessageQueue::node_size(), queue_msg_count)
    {
    }

    /**
     * @brief Sets the periodic task interval.
     * @param interval_ms The interval in milliseconds.
     */
    void set_periodic_task_interval(std::chrono::milliseconds interval_ms)
    {
      periodic_task_interval_ms = interval_ms;
    }

    /**
     * @brief Virtual function to be overridden by derived classes for periodic task processing.
     *
     * This function is called periodically based on the set interval and will not get called
     * if the interval is set to 0. Derived classes should implement this function to define
     * if they need periodic behavior of the task.
     */
    virtual void periodic_task_process() {}

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
     * @brief Performs initialization tasks.
     *
     * This virtual function is called during the creation process (via Task::create) to perform any
     * necessary initialization, such as subscribing to the appropriate message types. Derived
     * classes must implement this function to perform any initialization tasks specific to the
     * task.
     */
    virtual void on_initialize() = 0;

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

    /**
     * @brief Handles heartbeat messages.
     *
     * This method is called when a heartbeat message is received. Derived classes can
     * override this method to implement custom behavior for handling heartbeat messages.
     *
     * @param heartbeat_msg The heartbeat message to handle.
     */
    virtual void handle_heartbeat(const msg::HeartbeatMsg* heartbeat_msg);

    /**
     * @brief Gets the message queue for this task.
     * @return A reference to the message queue.
     */
    MessageQueue& get_message_queue() { return message_queue; }

  private:
    std::atomic<bool> running;      ///< Flag indicating if the task is running.
    std::thread       queue_thread; ///< The thread in which the task runs and processes messages.
    std::thread       periodic_thread; ///< The thread for periodic task execution. Only starts if
                                       ///< periodic_task_interval_ms is set.
    std::chrono::milliseconds periodic_task_interval_ms =
        std::chrono::milliseconds(0); ///< The interval for periodic tasks in milliseconds.
    MessageQueue message_queue;       ///< The message queue for storing incoming messages.

    /**
     * @brief The main execution loop for processing messages
     */
    void run();

    /**
     * @brief The execution loop for periodic tasks. Does not run if periodic_task_interval_ms is 0.
     */
    void run_periodic();
  };
} // namespace task