# ProtoCore

ProtoCore represents the starting point of advanced software design—a modular and efficient framework at the core of robotics and task management. Built with modern C++, ProtoCore leverages the language's performance and flexibility to enable precise state management, robust message-based communication, and modular task coordination. "Proto" symbolizes both the foundation of core functionality and the promise of new possibilities, ever-expanding to meet the demands of innovation.

---

## Features

ProtoCore provides modular functionalities for managing tasks, coordinating state transitions, and facilitating communication through a robust message-passing system:

- **Task Management**:  
  Tasks in ProtoCore are modular units equipped with state management, message-based communication, and customizable periodic processing capabilities. Each task operates within a finite state machine (NOT_STARTED, IDLE, RUNNING, STOPPED, ERROR), transitioning between states. Tasks process messages asynchronously through their process_message() method and can perform periodic operations via periodic_task_process().

- **Message Queues**:  
  ProtoCore implements efficient inter-process communication through priority-based message queues backed by pre-allocated memory pools. Each message queue guarantees deterministic memory usage with O(log n) priority-based message processing.

- **Publisher-Subscriber Model**:  
  A central Broker class manages message routing with a typed subscription system. Tasks can publish messages to be delivered to all subscribers of that message type, ensuring loosely coupled operations.

- **Memory Management**:  
  Uses custom memory pools to eliminate heap fragmentation and provide deterministic allocation/deallocation. Memory is pre-allocated during system initialization to ensure predictable runtime behavior.

- **State Management**:  
  A centralized StateManager oversees and synchronizes state transitions across tasks. It supports both asynchronous state requests and synchronous state demands with timeout capabilities.

- **Prioritized Message Handling**:  
  Messages are processed based on priority.

- **Health Monitoring**:
  The HeartBeatTask monitors registered tasks by sending periodic heartbeats and tracking responses. Unresponsive tasks can be automatically transitioned to ERROR state.

---
## Architecture Overview
ProtoCore uses a task-based architecture where each component is a task with its own execution thread:

Tasks register with the StateManager during system initialization
The StateManager coordinates task state transitions (`IDLE` -> `RUNNING` -> `STOPPED`)
Tasks communicate using a publisher-subscriber pattern through the Broker
Message queues efficiently deliver messages with priority handling
The HeartBeatTask monitors system health by sending periodic heartbeats
Memory management is handled through custom memory pools for deterministic behavior

## Core Components
### Task System
Tasks are the fundamental building blocks of ProtoCore applications. Each task:

- Runs in its own thread to process messages from its queue
- Can execute periodic operations in a separate thread if enabled with `periodic_task_interval`
- Follows a state machine with transitions coordinated by the StateManager

```c++
// Example of a simple task
class MyTask : public task::Task
{
public:
  static std::shared_ptr<MyTask> create(const std::string &name)
  {
    auto instance = std::shared_ptr<MyTask>(new MyTask(name));
    instance->on_initialize();
    return instance;
  }

protected:
  MyTask(const std::string &name) : Task(name) { }
  
  void on_initialize() override
  {
    safe_subscribe(msg::Type::StateMsg);
    safe_subscribe(msg::Type::MyCustomMsg);
  }
  
  void process_message(const msg::Msg &msg) override
  {
    if (auto state_msg = msg.get_data_as<msg::StateMsg>())
    {
      transition_to_state(static_cast<task::TaskState>(state_msg->state));
    }
    else if (auto custom_msg = msg.get_data_as<msg::MyCustomMsg>())
    {
      // Handle custom message
    }
  }
  
  void periodic_task_process() override
  {
    // Perform periodic operations
    // This runs at the interval specified by set_periodic_task_interval()
  }
};
```
### Task System
Messages are typed containers with priorities:

- Define message types using the DECLARE_MESSAGE_TYPE macro
- Messages are wrapped in a msg::Msg container for delivery
- Each message type has an associated priority level
- Tasks access message data through type-safe casting

```c++
// Example of defining a custom message
namespace msg
{
  DECLARE_MESSAGE_TYPE(MyCustomMsg)
  {
    uint32_t data_field1;
    uint64_t timestamp;
    float value;
  };
}

// Creating and sending a message
void send_example()
{
  safe_publish(msg::Msg(this, msg::MyCustomMsg{42, get_timestamp(), 3.14f}));
}
```
### Memory Management
ProtoCore uses memory pools to guarantee deterministic memory behavior:

- Memory is pre-allocated at initialization time
- Message queues use pool-based allocation for messages
- No dynamic memory allocation during normal operation
- Fixed-size pools eliminate heap fragmentation

### Heartbeat Monitoring
The HeartBeatTask provides health monitoring:

1. Tasks register with the HeartBeatTask via StateManager
2. HeartBeatTask sends periodic heartbeat messages
3. Tasks respond with acknowledgments
4. Unresponsive tasks trigger ERROR

```c++
// Setting up heartbeat monitoring
auto state_manager = StateManager::create();
auto heartbeat_task = HeartBeatTask::create(
    "HeartBeatTask", 
    state_manager,
    std::chrono::milliseconds(1000),  // Heartbeat interval
    std::chrono::milliseconds(3000)   // Response timeout
);


// Register tasks with StateManager
state_manager->set_task_registration_observer(heartbeat_task);
state_manager->register_task(heartbeat_task);
state_manager->register_task(my_task);
```

## Usage Examples
#### Creating A new task
```c++
// 1. Define your task class
class SensorTask : public task::Task
{
public:
  static std::shared_ptr<SensorTask> create(
      const std::string &name, 
      std::chrono::milliseconds update_interval = std::chrono::milliseconds(100)) 
  {
    auto instance = std::shared_ptr<SensorTask>(new SensorTask(name));
    instance->set_periodic_task_interval(update_interval);
    instance->on_initialize();
    return instance;
  }

protected:
  SensorTask(const std::string &name) : Task(name) {}
  
  void on_initialize() override
  {
    safe_subscribe(msg::Type::StateMsg);
    safe_subscribe(msg::Type::SensorConfigMsg);
  }
  
  void process_message(const msg::Msg &msg) override
  {
    if (auto state_msg = msg.get_data_as<msg::StateMsg>())
    {
      transition_to_state(static_cast<task::TaskState>(state_msg->state));
    }
    else if (auto config_msg = msg.get_data_as<msg::SensorConfigMsg>())
    {
      // Apply sensor configuration
    }
  }
  
  void periodic_task_process() override
  {
    // Read sensor and publish data
    if (get_current_state() == task::TaskState::RUNNING)
    {
      // Read sensor
      // msg::SensorDataMsg data{...};
      // safe_publish(msg::Msg(this, data));
    }
  }
};

// 2. Register with StateManager
auto state_manager = StateManager::create();
auto sensor_task = SensorTask::create("MainSensor");
state_manager->register_task(sensor_task);

// 3. Initialize and start the system
state_manager->initialize();  // Transition tasks to IDLE
state_manager->request_state_transition(task::TaskState::RUNNING);  // Start tasks
```
#### Defining A Custom Message
```c++
// 1. Define your message type in a header file
namespace msg
{
  DECLARE_MESSAGE_TYPE(SensorDataMsg)
  {
    uint64_t timestamp;
    float temperature;
    float humidity;
    float pressure;
  };
  
  DECLARE_MESSAGE_TYPE(SensorConfigMsg)
  {
    uint32_t sample_rate_hz;
    bool enable_filtering;
    float filter_cutoff;
  };
}

// 2. Create and send the message
void send_sensor_reading()
{
  uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  
  msg::SensorDataMsg dat
  {
    now,           // timestamp
    22.5f,         // temperature
    65.0f,         // humidity
    1013.25f       // pressure
  };
  
  safe_publish(msg::Msg(this, data));
}
```

## Building ProtoCore
### Prerequisites
- C++ 17 compiler (GCC 11.4.0 or compatible).
- Cmake
- gtest (dependencies added with cmake)

### Build Instructions
The project uses `CMake` for building and managing dependencies.
1. Clone the repository.
2. Configure the build:
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
   ```
3. Build the project:
   ```bash
   cmake --build build
   ```

### Running Tests
ProtoCore uses Google Test for comprehensive unit testing. Tests are organized by component:

- Message Tests: Verify message creation, priorities, and type safety
- Memory Pool Tests: Validate allocation, deallocation, and thread safety
- Task Tests: Test task lifecycle, message processing, and periodic execution
- Broker Tests: Verify message routing and subscription management
- State Manager Tests: Test state transitions and task coordination
- Heartbeat Tests: Validate health monitoring and unresponsive task handling
```bash
cd build
ctest
```

## TODO
- Logging
  - Develop a logger to capture all internal messaging and print outputs.
  - Design Requirements:
    - **Reentrant and Thread-Safe**: Must safely handle concurrent logging calls from multiple threads.
    - **Flexible Output**: Support both logging to a file and establishing a socket connection for data transmission.
    - **Configurable Levels**: Include support for different log levels such as INFO, DEBUG, WARN, and ERROR.
    - **Structured Logging**: Provide clear, structured output that can log task states, message content, and system events.
    - **Performance**: Implement asynchronous logging to minimize performance impact.