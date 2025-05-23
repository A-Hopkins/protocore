#include "broker.h"
#include "msg/msg.h"
#include "state_manager.h"
#include "task.h"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <thread>

// MockTask for testing.
class MockTask : public task::Task
{
public:
  /**
   * @brief Factory method for creating a MockTask instance.
   *
   * @param name The name of the task.
   * @return std::shared_ptr<MockTask> A shared pointer to the newly created MockTask.
   */
  static std::shared_ptr<MockTask> create(const std::string& name)
  {
    auto instance = std::shared_ptr<MockTask>(new MockTask(name));
    instance->on_initialize();
    return instance;
  }

protected:
  // Protected constructor to enforce factory use.
  MockTask(const std::string& name) : task::Task(name) {}

public:
  void process_message(const msg::Msg& msg) override
  {
    // If the message contains a state message, transition accordingly.
    if (auto stateMsg = msg.get_data_as<msg::StateMsg>())
    {
      transition_to_state(static_cast<task::TaskState>(stateMsg->state));
    }
  }

protected:
  void on_initialize() override
  {
    // Safely subscribe to state messages.
    safe_subscribe(msg::Type::StateMsg);
  }
};

// DelayedAckMockTask delays its acknowledgment.
class DelayedAckMockTask : public MockTask
{
public:
  /**
   * @brief Factory method for creating a DelayedAckMockTask instance.
   *
   * @param name The name of the task.
   * @param delay The delay before acknowledging the state transition.
   * @return std::shared_ptr<DelayedAckMockTask> A shared pointer to the new instance.
   */
  static std::shared_ptr<DelayedAckMockTask> create(const std::string&        name,
                                                    std::chrono::milliseconds delay)
  {
    auto instance = std::shared_ptr<DelayedAckMockTask>(new DelayedAckMockTask(name, delay));
    instance->on_initialize();
    return instance;
  }

  DelayedAckMockTask(const std::string& name, std::chrono::milliseconds delay)
    : MockTask(name), delay_(delay)
  {
  }

  void process_message(const msg::Msg& msg) override
  {
    auto stateMsg = msg.get_data_as<msg::StateMsg>();
    if (!stateMsg)
      return;

    task::TaskState new_state = static_cast<task::TaskState>(stateMsg->state);
    if (new_state == task::TaskState::STOPPED)
    {
      transition_to_state(new_state);
      return;
    }

    if (get_current_state() == task::TaskState::NOT_STARTED ||
        get_current_state() == task::TaskState::IDLE)
    {
      // Process immediately during initialization.
      transition_to_state(static_cast<task::TaskState>(stateMsg->state));
    }
    else
    {
      // For subsequent transitions, delay only briefly.
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_));
      transition_to_state(static_cast<task::TaskState>(stateMsg->state));
    }
  }

private:
  std::chrono::milliseconds delay_;
};

// NonResponsiveMockTask ignores state transitions.
class NonResponsiveMockTask : public MockTask
{
public:
  /**
   * @brief Factory method for creating a NonResponsiveMockTask instance.
   *
   * @param name The name of the task.
   * @return std::shared_ptr<NonResponsiveMockTask> A shared pointer to the new instance.
   */
  static std::shared_ptr<NonResponsiveMockTask> create(const std::string& name)
  {
    auto instance = std::shared_ptr<NonResponsiveMockTask>(new NonResponsiveMockTask(name));
    instance->on_initialize();
    return instance;
  }

  NonResponsiveMockTask(const std::string& name) : MockTask(name) {}

  void process_message(const msg::Msg& msg) override
  {
    auto stateMsg = msg.get_data_as<msg::StateMsg>();
    if (!stateMsg)
      return;
    task::TaskState new_state = static_cast<task::TaskState>(stateMsg->state);

    if (new_state == task::TaskState::STOPPED)
    {
      transition_to_state(new_state);
      return;
    }
    if (get_current_state() == task::TaskState::NOT_STARTED)
    {
      transition_to_state(static_cast<task::TaskState>(stateMsg->state));
    }
  }
};

// --- Test Fixture for StateManager Tests ---

class StateManagerTest : public ::testing::Test
{
protected:
  void SetUp() override { Broker::initialize(); }

  void TearDown() override
  {
    // Allow a short delay for threads to settle between tests.
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
};

TEST_F(StateManagerTest, RegisterTask)
{
  auto task = MockTask::create("TaskRegistration");
  auto sm   = StateManager::create();

  sm->register_task(task);

  // Indirectly verify registration via initialize.
  sm->initialize();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_EQ(task->get_current_state(), task::TaskState::IDLE);

  // Shutdown the state manager to clean up its thread.
  sm->shutdown();
}

TEST_F(StateManagerTest, RequestStateTransition)
{
  auto task = MockTask::create("TaskRegistration");
  auto sm   = StateManager::create();

  sm->register_task(task);
  sm->initialize();

  // Request a transition to RUNNING.
  sm->request_state_transition(task::TaskState::RUNNING);

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_EQ(task->get_current_state(), task::TaskState::RUNNING);

  sm->shutdown();
}

TEST_F(StateManagerTest, DemandStateTransitionSuccess)
{
  auto task1 = MockTask::create("Task1");
  auto task2 = MockTask::create("Task2");
  auto sm    = StateManager::create();

  sm->register_task(task1);
  sm->register_task(task2);

  sm->initialize();

  bool success = sm->demand_state_transition(task::TaskState::RUNNING, std::chrono::seconds(10));

  EXPECT_TRUE(success);
  EXPECT_EQ(task1->get_current_state(), task::TaskState::RUNNING);
  EXPECT_EQ(task2->get_current_state(), task::TaskState::RUNNING);

  sm->shutdown();
}

TEST_F(StateManagerTest, DemandStateTransitionTimeout)
{
  auto task1 = MockTask::create("ResponsiveTask");
  auto task2 = NonResponsiveMockTask::create("NonResponsiveTask");
  auto sm    = StateManager::create();

  sm->register_task(task1);
  sm->register_task(task2);

  sm->initialize();

  bool success =
      sm->demand_state_transition(task::TaskState::RUNNING, std::chrono::milliseconds(10));

  EXPECT_FALSE(success);
  EXPECT_EQ(task1->get_current_state(), task::TaskState::RUNNING);
  EXPECT_NE(task2->get_current_state(), task::TaskState::RUNNING);

  sm->shutdown();
}

TEST_F(StateManagerTest, InitializeWithMultipleTasks)
{
  auto task1 = MockTask::create("InitTask1");
  auto task2 = MockTask::create("InitTask2");
  auto task3 = MockTask::create("InitTask3");

  auto sm = StateManager::create();

  sm->register_task(task1);
  sm->register_task(task2);
  sm->register_task(task3);

  sm->initialize();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  EXPECT_EQ(task1->get_current_state(), task::TaskState::IDLE);
  EXPECT_EQ(task2->get_current_state(), task::TaskState::IDLE);
  EXPECT_EQ(task3->get_current_state(), task::TaskState::IDLE);

  sm->shutdown();
}

TEST_F(StateManagerTest, ShutdownWithMultipleTasks)
{
  auto task1 = MockTask::create("ShutdownTask1");
  auto task2 = MockTask::create("ShutdownTask2");

  auto sm = StateManager::create();

  sm->register_task(task1);
  sm->register_task(task2);

  sm->initialize();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  sm->shutdown();
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  EXPECT_EQ(task1->get_current_state(), task::TaskState::STOPPED);
  EXPECT_EQ(task2->get_current_state(), task::TaskState::STOPPED);
}

TEST_F(StateManagerTest, DemandStateTransitionWithDelayedAck)
{
  auto task1 = MockTask::create("NormalTask");
  auto task2 = DelayedAckMockTask::create("DelayedTask", std::chrono::milliseconds(100));
  auto sm    = StateManager::create();

  sm->register_task(task1);
  sm->register_task(task2);

  sm->initialize();

  bool success =
      sm->demand_state_transition(task::TaskState::RUNNING, std::chrono::milliseconds(101));

  EXPECT_TRUE(success);
  EXPECT_EQ(task1->get_current_state(), task::TaskState::RUNNING);
  EXPECT_EQ(task2->get_current_state(), task::TaskState::RUNNING);

  sm->shutdown();
}

TEST_F(StateManagerTest, MultipleStateTransitions)
{
  auto task = MockTask::create("MultiTransitionTask");
  auto sm   = StateManager::create();

  sm->register_task(task);
  sm->initialize();

  sm->demand_state_transition(task::TaskState::IDLE, std::chrono::milliseconds(11));
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_EQ(task->get_current_state(), task::TaskState::IDLE);

  sm->demand_state_transition(task::TaskState::RUNNING, std::chrono::milliseconds(11));
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_EQ(task->get_current_state(), task::TaskState::RUNNING);

  sm->demand_state_transition(task::TaskState::IDLE, std::chrono::milliseconds(11));
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_EQ(task->get_current_state(), task::TaskState::IDLE);

  sm->demand_state_transition(task::TaskState::STOPPED, std::chrono::milliseconds(11));
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_EQ(task->get_current_state(), task::TaskState::STOPPED);

  sm->demand_state_transition(task::TaskState::ERROR, std::chrono::milliseconds(11));
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  EXPECT_EQ(task->get_current_state(), task::TaskState::ERROR);

  sm->shutdown();
}

TEST_F(StateManagerTest, ErrorStateHandling)
{
  auto task = MockTask::create("ErrorTask");
  auto sm   = StateManager::create();

  sm->register_task(task);
  sm->initialize();

  sm->demand_state_transition(task::TaskState::ERROR, std::chrono::milliseconds(11));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_EQ(task->get_current_state(), task::TaskState::ERROR);

  sm->demand_state_transition(task::TaskState::IDLE, std::chrono::milliseconds(11));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_EQ(task->get_current_state(), task::TaskState::IDLE);

  sm->shutdown();
}

TEST_F(StateManagerTest, MarkTaskAsUnresponsive)
{
  auto task1 = MockTask::create("ResponsiveTask");
  auto task2 = MockTask::create("UnresponsiveTask");
  auto sm    = StateManager::create();

  sm->register_task(task1);
  sm->register_task(task2);

  sm->initialize();
  sm->request_state_transition(task::TaskState::RUNNING);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // Both tasks should be RUNNING initially
  EXPECT_EQ(task1->get_current_state(), task::TaskState::RUNNING);
  EXPECT_EQ(task2->get_current_state(), task::TaskState::RUNNING);

  // Mark only task2 as unresponsive
  sm->mark_task_as_unresponsive(task2);
  std::this_thread::sleep_for(std::chrono::milliseconds(5));

  // task1 should still be RUNNING, task2 should be ERROR
  EXPECT_EQ(task1->get_current_state(), task::TaskState::RUNNING);
  EXPECT_EQ(task2->get_current_state(), task::TaskState::ERROR);

  sm->shutdown();
}
