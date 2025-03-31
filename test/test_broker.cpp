#include <gtest/gtest.h>
#include "msg/msg.h"
#include "broker.h"
#include "task.h"

// Mock task class for testing
class MockTask : public task::Task
{
public:
  MockTask(const std::string& name) : Task() { }
  
  std::vector<msg::Msg> received_messages;
  
  // Process just a single message to avoid memory issues
  bool process()
  {
    std::optional<msg::Msg> msg = message_queue.try_dequeue();
    if (msg)
    {
      received_messages.push_back(*msg);
      return true;
    }
    return false;
  }
  };

TEST(BrokerTest, Initialization)
{
  // Test initialization
  Broker::initialize();
  
  // Can't easily test private members directly, but we can test indirectly
  // by subscribing and publishing
  auto task = std::make_shared<MockTask>("test_task");
  Broker::subscribe(task, msg::Type::HeartbeatMsg);
  
  msg::HeartbeatMsg heartbeat;
  msg::Msg message(task.get(), heartbeat);
  Broker::publish(message);
  
  task->process();
  EXPECT_EQ(task->received_messages.size(), 1);
}

TEST(BrokerTest, CustomSubscriberLimit)
{
  // Re-initialize with custom limit
  Broker::initialize(32);
  
  // Create tasks using pointers to avoid vector reallocation issues
  std::vector<std::shared_ptr<MockTask>> tasks;
  for (int i = 0; i < 30; i++)
  {
    tasks.emplace_back(std::make_shared<MockTask>("task_" + std::to_string(i)));
    Broker::subscribe(tasks.back(), msg::Type::StateMsg);
  }
  
  msg::StateMsg state_msg;
  msg::Msg message(nullptr, state_msg);
  Broker::publish(message);
  
  // Verify all tasks received the message
  for (auto& task_ptr : tasks)
  {
    task_ptr->process();
    EXPECT_EQ(task_ptr->received_messages.size(), 1);
  }
}

TEST(BrokerTest, MessageRouting)
{
  Broker::initialize();
  
  // Create tasks using shared pointers
  auto heartbeat_task = std::make_shared<MockTask>("heartbeat_subscriber");
  auto state_task = std::make_shared<MockTask>("state_subscriber");
  auto both_task = std::make_shared<MockTask>("both_subscriber");
  
  // Subscribe to different message types
  Broker::subscribe(heartbeat_task, msg::Type::HeartbeatMsg);
  Broker::subscribe(state_task, msg::Type::StateMsg);
  Broker::subscribe(both_task, msg::Type::HeartbeatMsg);
  Broker::subscribe(both_task, msg::Type::StateMsg);
  
  // Test heartbeat message
  {
    msg::HeartbeatMsg heartbeat;
    msg::Msg heartbeat_msg(nullptr, heartbeat);
    Broker::publish(heartbeat_msg);
    
    // Process messages
    heartbeat_task->process();
    both_task->process();
  }
  
  // Test state message
  {
    msg::StateMsg state;
    msg::Msg state_msg(nullptr, state);
    Broker::publish(state_msg);
    
    // Process messages
    state_task->process();
    both_task->process();
  }
  
  // Verify routing
  EXPECT_EQ(heartbeat_task->received_messages.size(), 1);
  EXPECT_EQ(state_task->received_messages.size(), 1);
  EXPECT_EQ(both_task->received_messages.size(), 2);
}