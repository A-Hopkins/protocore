#include "broker.h"
#include "heart_beat.h"
#include "msg/system_msgs.h"
#include "state_manager.h"
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

/**
 * @brief Mock task that responds to heartbeats
 */
class HeartbeatResponsiveTask : public task::Task
{
public:
  static std::shared_ptr<HeartbeatResponsiveTask> create(const std::string& name)
  {
    auto instance = std::shared_ptr<HeartbeatResponsiveTask>(new HeartbeatResponsiveTask(name));
    instance->on_initialize();
    return instance;
  }

  // Track heartbeats received
  std::vector<msg::HeartbeatMsg> received_heartbeats;
  bool                           should_respond = true;

protected:
  HeartbeatResponsiveTask(const std::string& name) : Task(name) {}

  void on_initialize() override
  {
    safe_subscribe(msg::Type::StateMsg);
    safe_subscribe(msg::Type::HeartbeatMsg);
  }

  void process_message(const msg::Msg& msg) override
  {
    if (auto state_msg = msg.get_data_as<msg::StateMsg>())
    {
      transition_to_state(static_cast<task::TaskState>(state_msg->state));
      return;
    }

    if (auto heartbeat = msg.get_data_as<msg::HeartbeatMsg>())
    {
      received_heartbeats.push_back(*heartbeat);

      if (should_respond)
      {
        // Send acknowledgment
        msg::HeartbeatAckMsg ack;
        ack.orig_unique_id = heartbeat->unique_id;
        ack.orig_timestamp = heartbeat->timestamp;
        ack.ack_timestamp  = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                .count();
        safe_publish(msg::Msg(this, ack));
      }
    }
  }
};

/**
 * @brief Mock task that never responds to heartbeats
 */
class HeartbeatNonResponsiveTask : public task::Task
{
public:
  static std::shared_ptr<HeartbeatNonResponsiveTask> create(const std::string& name)
  {
    auto instance =
        std::shared_ptr<HeartbeatNonResponsiveTask>(new HeartbeatNonResponsiveTask(name));
    instance->on_initialize();
    return instance;
  }

  // Track heartbeats received
  std::vector<msg::HeartbeatMsg> received_heartbeats;

protected:
  HeartbeatNonResponsiveTask(const std::string& name) : Task(name) {}

  void on_initialize() override
  {
    safe_subscribe(msg::Type::StateMsg);
    safe_subscribe(msg::Type::HeartbeatMsg);
  }

  void process_message(const msg::Msg& msg) override
  {
    if (auto state_msg = msg.get_data_as<msg::StateMsg>())
    {
      transition_to_state(static_cast<task::TaskState>(state_msg->state));
      return;
    }

    if (auto heartbeat = msg.get_data_as<msg::HeartbeatMsg>())
    {
      received_heartbeats.push_back(*heartbeat);
    }
  }
};

/**
 * @brief Test fixture for HeartBeat tests
 */
class HeartBeatTaskTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    Broker::initialize();
    state_manager = StateManager::create();

    // Create HeartBeat task with short timeouts for testing
    heart_beat = HeartBeatTask::create("TestHeartBeat", state_manager,
                                       std::chrono::milliseconds(10), // Heartbeat every 10ms
                                       std::chrono::milliseconds(30)  // Response timeout of 30ms
    );

    responsive_task     = HeartbeatResponsiveTask::create("ResponsiveTask");
    non_responsive_task = HeartbeatNonResponsiveTask::create("NonResponsiveTask");

    // Register the heartbeat task as an observer
    state_manager->set_task_registration_observer(heart_beat);

    // Register tasks with the state manager
    state_manager->register_task(heart_beat);
    state_manager->register_task(responsive_task);
    state_manager->register_task(non_responsive_task);

    // Initialize the state manager (transitions tasks to IDLE)
    state_manager->initialize();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  void TearDown() override
  {
    // Ensure all tasks are stopped
    state_manager->shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  // Helper method to transition tasks to RUNNING
  void start_tasks()
  {
    state_manager->request_state_transition(task::TaskState::RUNNING);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  std::shared_ptr<StateManager>               state_manager;
  std::shared_ptr<HeartBeatTask>              heart_beat;
  std::shared_ptr<HeartbeatResponsiveTask>    responsive_task;
  std::shared_ptr<HeartbeatNonResponsiveTask> non_responsive_task;
};

TEST_F(HeartBeatTaskTest, HeartbeatsSentWhenRunning)
{
  // Start from a clean slate
  start_tasks();

  // Let the heartbeat task run
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // Verify heartbeats were sent to both tasks
  EXPECT_GT(responsive_task->received_heartbeats.size(), 0);
  EXPECT_GT(non_responsive_task->received_heartbeats.size(), 0);
}

TEST_F(HeartBeatTaskTest, NoHeartbeatsWhenNotRunning)
{
  // Note: We don't call start_tasks() - tasks remain in IDLE

  // Record current heartbeat counts
  size_t responsive_count     = responsive_task->received_heartbeats.size();
  size_t non_responsive_count = non_responsive_task->received_heartbeats.size();

  // Wait a bit
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // No new heartbeats should be sent
  EXPECT_EQ(responsive_task->received_heartbeats.size(), responsive_count);
  EXPECT_EQ(non_responsive_task->received_heartbeats.size(), non_responsive_count);
}

TEST_F(HeartBeatTaskTest, ResponsiveTaskStaysHealthy)
{
  // Start tasks
  start_tasks();

  // Let the heartbeat task run
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Verify responsive task stays in RUNNING state
  EXPECT_EQ(responsive_task->get_current_state(), task::TaskState::RUNNING);
}

TEST_F(HeartBeatTaskTest, UnresponsiveTaskHandling)
{
  // Start tasks
  start_tasks();

  // Let the heartbeat task run long enough for timeout detection
  std::this_thread::sleep_for(std::chrono::milliseconds(60));

  // Verify unresponsive task was transitioned to ERROR state
  EXPECT_EQ(non_responsive_task->get_current_state(), task::TaskState::ERROR);
}

TEST_F(HeartBeatTaskTest, TaskBecomingUnresponsive)
{
  // Start tasks
  start_tasks();

  // Let the heartbeat task run briefly
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // Verify responsive task is still healthy
  EXPECT_EQ(responsive_task->get_current_state(), task::TaskState::RUNNING);

  // Make the task unresponsive
  responsive_task->should_respond = false;

  // Let the heartbeat task detect the unresponsive task
  std::this_thread::sleep_for(std::chrono::milliseconds(60));

  // Verify task was transitioned to ERROR state
  EXPECT_EQ(responsive_task->get_current_state(), task::TaskState::ERROR);
}
