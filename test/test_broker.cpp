#include "broker.h"
#include "msg/msg.h"
#include "task.h"
#include <gtest/gtest.h>

// Mock task class for testing
class MockTask : public task::Task
{
public:
  // Public factory method for MockTask so tests can create instances.
  static std::shared_ptr<MockTask> create(const std::string& name)
  {
    auto instance = std::shared_ptr<MockTask>(new MockTask(name));
    instance->on_initialize();
    return instance;
  }

  MockTask(const std::string& name) : Task(name) {}

  std::vector<msg::Msg> received_messages;

  // Process a single message from the queue using a protected accessor.
  bool process()
  {
    std::optional<msg::Msg> msgOpt = get_message_queue().try_dequeue();
    if (msgOpt)
    {
      received_messages.push_back(*msgOpt);
      return true;
    }
    return false;
  }

protected:
  // Implement the required virtual method from Task class.
  void process_message(const msg::Msg& msg) override { received_messages.push_back(msg); }

  // Provide an implementation of on_initialize() as required.
  void on_initialize() override {}
};

TEST(BrokerTest, Initialization)
{
  // Test initialization.
  Broker::initialize();

  // Create the task via the factory on the derived class.
  auto task = MockTask::create("test_task");
  Broker::subscribe(task, msg::Type::HeartbeatMsg);

  msg::HeartbeatMsg heartbeat;
  msg::Msg          message(task.get(), heartbeat);
  Broker::publish(message);

  // Process messages.
  task->process();
  EXPECT_EQ(task->received_messages.size(), 1);
}

TEST(BrokerTest, CustomSubscriberLimit)
{
  // Re-initialize with custom limit.
  Broker::initialize(32);

  // Create tasks using the factory method on the derived class.
  std::vector<std::shared_ptr<MockTask>> tasks;
  for (int i = 0; i < 30; i++)
  {
    tasks.emplace_back(MockTask::create("task_" + std::to_string(i)));
    Broker::subscribe(tasks.back(), msg::Type::StateMsg);
  }

  msg::StateMsg state_msg;
  msg::Msg      message(nullptr, state_msg);
  Broker::publish(message);

  // Verify all tasks received the message.
  for (auto& task_ptr : tasks)
  {
    task_ptr->process();
    EXPECT_EQ(task_ptr->received_messages.size(), 1);
  }
}

TEST(BrokerTest, MessageRouting)
{
  Broker::initialize();

  // Create tasks using the factory method on the derived class.
  auto heartbeat_task = MockTask::create("heartbeat_subscriber");
  auto state_task     = MockTask::create("state_subscriber");
  auto both_task      = MockTask::create("both_subscriber");

  // Subscribe to different message types.
  Broker::subscribe(heartbeat_task, msg::Type::HeartbeatMsg);
  Broker::subscribe(state_task, msg::Type::StateMsg);
  Broker::subscribe(both_task, msg::Type::HeartbeatMsg);
  Broker::subscribe(both_task, msg::Type::StateMsg);

  // Test heartbeat message.
  {
    msg::HeartbeatMsg heartbeat;
    msg::Msg          heartbeat_msg(nullptr, heartbeat);
    Broker::publish(heartbeat_msg);

    heartbeat_task->process();
    both_task->process();
  }

  // Test state message.
  {
    msg::StateMsg state;
    msg::Msg      state_msg(nullptr, state);
    Broker::publish(state_msg);

    state_task->process();
    both_task->process();
  }

  // Verify routing.
  EXPECT_EQ(heartbeat_task->received_messages.size(), 1);
  EXPECT_EQ(state_task->received_messages.size(), 1);
  EXPECT_EQ(both_task->received_messages.size(), 2);
}