#include <gtest/gtest.h>
#include <chrono>
#include <atomic>
#include <thread>
#include "task.h"
#include "msg/system_msgs.h"  // include system messages for state messages
#include "broker.h"

// Test fixture to initialize the Broker.
class TaskTestFixture : public ::testing::Test
{
protected:
  void SetUp() override
  {
    Broker::initialize();
  }
  
  void TearDown() override
  {
    // Allow time for threads to settle between tests.
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
};

// Derived Task for testing.
class MockTask : public task::Task
{
public:
  std::atomic<int> periodic_count{0};
  std::atomic<int> msg_count{0};

  /**
   * @brief Factory method for creating a MockTask instance.
   *
   * @param task_name The name of the task.
   * @param interval The periodic execution interval in milliseconds.
   * @return std::shared_ptr<MockTask> A shared pointer to the newly created MockTask.
   */
  static std::shared_ptr<MockTask> create(const std::string& task_name, unsigned int interval = 0)
  {
    auto instance = std::shared_ptr<MockTask>(new MockTask(task_name, interval));
    instance->on_initialize();
    return instance;
  }

protected:
  // Protected constructor to enforce use of the factory method.
  MockTask(const std::string& task_name, unsigned int interval = 0)
    : Task(task_name)
  {
    if(interval > 0)
    {
      set_periodic_task_interval(interval);
    }
  }
  
  // Implement on_initialize() so that MockTask is not abstract.
  void on_initialize() override { safe_subscribe(msg::Type::StateMsg); }
  
  void process_message(const msg::Msg& m) override
  {
    // If the message contains a state message, transition accordingly.
    if (auto stateMsg = m.get_data_as<msg::StateMsg>())
    {
      transition_to_state(static_cast<task::TaskState>(stateMsg->state));
    }
    ++msg_count;
  }
  
  void periodic_task_process() override
  {
    ++periodic_count;
  }
};

TEST_F(TaskTestFixture, StartStop)
{
  auto task = MockTask::create("TestTask");
  task->start();
  // Allow time for thread initialization.
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_TRUE(task->is_running());
  task->stop();
  EXPECT_FALSE(task->is_running());
}

TEST_F(TaskTestFixture, PeriodicExecution)
{
  auto task = MockTask::create("PeriodicTask", 5);
  task->start();
  // Wait long enough for periodic_task_process to be invoked.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  task->stop();
  EXPECT_GE(task->periodic_count.load(), 1);
}

TEST_F(TaskTestFixture, ProcessStateTransition)
{
  auto task = MockTask::create("StateTask");
  task->start();

  // Create a state message to transition to IDLE.
  msg::StateMsg stateMsg{ static_cast<uint8_t>(task::TaskState::IDLE) };
  msg::Msg message(task.get(), stateMsg);

  // Use the public deliver_message() method.
  task->deliver_message(message);

  // Allow time for the message to be processed.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // Verify that the task's current state is IDLE.
  EXPECT_EQ(task->get_current_state(), task::TaskState::IDLE);

  // Explicitly stop the task to ensure the run loop exits.
  task->stop();
}

TEST_F(TaskTestFixture, MultipleStartStop)
{
  auto task = MockTask::create("MultipleTask");
  task->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  EXPECT_TRUE(task->is_running());
  // Calling stop multiple times should be safe.
  task->stop();
  task->stop();
  EXPECT_FALSE(task->is_running());
}

TEST_F(TaskTestFixture, NoPeriodicExecutionWithZeroInterval)
{
  auto task = MockTask::create("NoPeriodicTask", 0);
  task->start();
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  task->stop();
  EXPECT_EQ(task->periodic_count.load(), 0);
}

TEST_F(TaskTestFixture, SafePublishTest)
{
  auto task = MockTask::create("SafePublishTask");
  task->start();

  // Use safe_publish to send a state message.
  msg::StateMsg stateMsg{ static_cast<uint8_t>(task::TaskState::RUNNING) };
  task->safe_publish(msg::Msg(task.get(), stateMsg));

  // Allow time for the message to be processed.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  EXPECT_EQ(task->get_current_state(), task::TaskState::RUNNING);

  task->stop();
}

TEST_F(TaskTestFixture, ShutdownPreventsFurtherProcessing)
{
  auto task = MockTask::create("ShutdownTask");
  task->start();

  // Stop the task.
  task->stop();

  // After shutdown, deliver a message.
  msg::StateMsg stateMsg{ static_cast<uint8_t>(task::TaskState::RUNNING) };
  task->deliver_message(msg::Msg(task.get(), stateMsg));

  // Allow time for the message (if any) to be processed.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // Expect that the task's state did not change after shutdown.
  EXPECT_NE(task->get_current_state(), task::TaskState::RUNNING);
}

TEST_F(TaskTestFixture, MultipleMessageProcessing)
{
  auto task = MockTask::create("MultiMessageTask");
  task->start();

  // Send a series of messages that change the state.
  msg::StateMsg idleMsg{ static_cast<uint8_t>(task::TaskState::IDLE) };
  msg::StateMsg runningMsg{ static_cast<uint8_t>(task::TaskState::RUNNING) };

  task->deliver_message(msg::Msg(task.get(), idleMsg));
  task->deliver_message(msg::Msg(task.get(), runningMsg));
  task->deliver_message(msg::Msg(task.get(), idleMsg));

  // Allow time for processing.
  std::this_thread::sleep_for(std::chrono::milliseconds(30));

  // Check that the last processed state is IDLE.
  EXPECT_EQ(task->get_current_state(), task::TaskState::IDLE);

  task->stop();
}

TEST_F(TaskTestFixture, ConcurrentEnqueueTest)
{
  auto task = MockTask::create("ConcurrentTask");
  task->start();

  constexpr int numMessages = 10;
  std::vector<std::thread> threads;

  // Launch multiple threads that concurrently publish messages.
  for (int i = 0; i < 10; i++)
  {
    threads.emplace_back([task, numMessages]() {
      for (int j = 0; j < numMessages; j++)
      {
        msg::StateMsg msgData{ static_cast<uint8_t>(task::TaskState::RUNNING) };
        task->safe_publish(msg::Msg(task.get(), msgData));
      }
    });
  }

  // Wait for all publisher threads to finish.
  for (auto& t : threads)
    t.join();

  // Allow time for all messages to be processed.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // We expect the task to have processed at least as many messages as sent.
  EXPECT_GE(task->msg_count.load(), 10 * numMessages);
  task->stop();
}
