# ProtoCore

ProtoCore represents the starting point of advanced software design—a modular and efficient framework at the core of robotics and task management. Built with modern C++, ProtoCore leverages the language’s performance and flexibility to enable precise state management, robust message-based communication, and modular task coordination. "Proto" symbolizes both the foundation of core functionality and the promise of new possibilities, ever-expanding to meet the demands of innovation.

---

## Features

ProtoCore provides modular functionalities for managing tasks, coordinating state transitions, and facilitating communication through a robust message-passing system:

- **Task Management**:  
  Tasks in ProtoCore are modular units equipped with state management, message-based communication, and customizable periodic processing capabilities. Each task operates within a finite state machine, transitioning between states. A task’s lifecycle and behavior can be managed through a `StateManager`, while its execution state is monitored using thread-safe mechanisms. Tasks include a  to facilitate inter-process communication, processing messages asynchronously through an overridden  method. Developers can also implement timed operations using the  function, with customizable intervals. Designed to be extensible, tasks allow custom logic for state transitions and message handling.

- **Message Queues**:  
  Facilitates efficient inter-process communication (IPC) through custom-designed message queues with statically allocated buffers, ensuring predictable memory usage and suitability for resource-constrained environments.

- **Publisher-Subscriber Model**:  
  Implements a pub/sub system for message-driven communication. Components can publish messages to be processed by subscribed tasks, ensuring loosely coupled operations.

- **Embedded System Considerations**:  
  Designed with resource-constrained systems in mind, ProtoCore minimizes heap usage and does not rely on exceptions from c++.

- **State Management**:  
  A centralized `StateManager` oversees and synchronizes state transitions, ensuring that tasks progress cohesively. A task is registered with the StateManager as a command and control operator that provides a single interface for controlling the system of tasks.

- **Prioritized Message Handling**:  
  Messages are processed based on priority.

- **Extensive Logging**:  
  Optional logging facilities help monitor task execution, debug communication flows, and trace state transitions.

---
## Architecture Overview
The architecture of ProtoCore revolves around the lifecycle of tasks, their integration with the `StateManager`, and their interaction with the message-passing system. Tasks are first registered with the , which coordinates their state transitions through the defined state machine. During operation, tasks communicate using a publisher-subscriber model, where messages are routed based on priority to ensure timely processing of critical information. Each task operates either periodically, based on a configurable interval, or only when a message is received on its message queue. The behavior of the task, including its message handling logic, is dependent on its current state within the state machine.
This design ensures modular, predictable, and efficient task execution while maintaining clear state coordination and inter-task communication.

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
Unit tests are implemented using Google Test. To run the tests:
```bash
cd build
ctest
```
