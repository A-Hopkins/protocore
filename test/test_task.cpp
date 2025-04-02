#include <gtest/gtest.h>
#include <chrono>
#include <atomic>
#include "task.h"
#include "msg/system_msgs.h"  // include system messages for state messages

// Derived Task for testing.
class MyTask : public task::Task
{
public:
  std::atomic<int> periodic_count{0};
  std::atomic<int> msg_count{0};

  MyTask(const std::string& task_name, unsigned int interval = 0)
    : Task(task_name)
  {
    if(interval > 0)
    {
      set_periodic_task_interval(interval);
    }
  }
protected:
  void process_message(const msg::Msg& m) override
  {
    // Check if message data is a state message.
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

TEST(TaskTest, StartStop)
{
  MyTask task("TestTask");
  task.start();
  // Allow time for thread initialization.
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  EXPECT_TRUE(task.is_running());
  task.stop();
  EXPECT_FALSE(task.is_running());
}

TEST(TaskTest, PeriodicExecution)
{
  MyTask task("PeriodicTask", 5);
  task.start();
  // Wait long enough for periodic_task_process to be invoked.
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  task.stop();
  EXPECT_GE(task.periodic_count.load(), 1);
}

TEST(TaskTest, ProcessStateTransition)
{
  MyTask task("StateTask");
  task.start();

  // Create a state message to transition to IDLE.
  msg::Msg message(&task, msg::StateMsg{static_cast<uint8_t>(task::TaskState::IDLE)});

  // Enqueue the state message.
  task.message_queue.enqueue(message);

  // Allow time for the message to be processed and the task to stop.
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  // Verify that the task's current state is IDLE.
  EXPECT_EQ(task.get_current_state(), task::TaskState::IDLE);
}

// Test for multiple start/stop calls.
TEST(TaskTest, MultipleStartStop)
{
  MyTask task("MultipleTask");
  task.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  EXPECT_TRUE(task.is_running());
  // Calling stop multiple times shouldn't cause issues.
  task.stop();
  task.stop();
  EXPECT_FALSE(task.is_running());
}

// Test that no periodic execution occurs when the interval is zero.
TEST(TaskTest, NoPeriodicExecutionWithZeroInterval)
{
  MyTask task("NoPeriodicTask", 0);
  task.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  task.stop();
  EXPECT_EQ(task.periodic_count.load(), 0);
}
