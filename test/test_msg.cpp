#include <gtest/gtest.h>
#include <queue>

#include "msg/msg.h"
#include "msg/system_msgs.h"

using namespace msg;

namespace task
{
  class Task
  {
  public:
    virtual ~Task() = default;
  };
}

// Mock Task class for testing - inherit from task::Task
struct MockTask : public task::Task
{
    // Provide any methods expected to be called on the Task in these tests
};

// Test for Msg priority ordering
TEST(MsgPriorityTest, PriorityOrdering)
{
  MockTask sender;
  Msg high_priority_msg = Msg::create<StateMsg>(&sender, 1);
  Msg low_priority_msg = Msg::create<HeartbeatMsg>(&sender, 12345, 67890);

  std::priority_queue<Msg> queue;
  queue.push(std::move(low_priority_msg));
  queue.push(std::move(high_priority_msg));

  ASSERT_EQ(queue.size(), 2);
  EXPECT_EQ(queue.top().get_priority(), 950); // Expect high priority first
  queue.pop();
  EXPECT_EQ(queue.top().get_priority(), 900); // Expect low priority second
}

// Test for StateMsg creation and data access
TEST(StateMsgTest, ConstructorAndDataAccess)
{
  MockTask sender;
  uint8_t state = 1;
  StateMsg state_msg(&sender, state);

  EXPECT_EQ(state_msg.get_type_id(), &StateMsg::type_id);
  EXPECT_EQ(state_msg.get_priority(), 950);
  EXPECT_EQ(state_msg.get_sender(), &sender);
  EXPECT_EQ(*state_msg.get_data(), state);
}

// Test for HeartbeatMsg creation and data access
TEST(HeartbeatMsgTest, ConstructorAndDataAccess)
{
  MockTask sender;
  uint32_t unique_id = 12345;
  uint32_t timestamp = 67890;
  HeartbeatMsg heartbeat_msg(&sender, unique_id, timestamp);

  EXPECT_EQ(heartbeat_msg.get_type_id(), &HeartbeatMsg::type_id);
  EXPECT_EQ(heartbeat_msg.get_priority(), 900);
  EXPECT_EQ(heartbeat_msg.get_sender(), &sender);
  EXPECT_EQ(heartbeat_msg.get_unique_id(), unique_id);
  EXPECT_EQ(heartbeat_msg.get_timestamp(), timestamp);
}

// Test for HeartbeatAckMsg creation and data access
TEST(HeartbeatAckMsgTest, AckMessage)
{
  MockTask sender;
  uint32_t unique_id = 54321;
  uint32_t timestamp = 98765;
  HeartbeatMsg heartbeat_msg(&sender, unique_id, timestamp);

  uint32_t ack_timestamp = 99999;
  HeartbeatAckMsg ack_msg(&sender, unique_id, timestamp, ack_timestamp);

  EXPECT_EQ(ack_msg.get_orig_unique_id(), unique_id);
  EXPECT_EQ(ack_msg.get_orig_timestamp(), timestamp);
  EXPECT_EQ(ack_msg.get_ack_timestamp(), ack_timestamp);
}

// Test for unique type identifiers
TEST(MsgUniqueIdsTest, UniqueTypeIds)
{
  MockTask sender;
  StateMsg state_msg(&sender, 1);
  StateAckMsg state_ack_msg(&sender, 2);
  HeartbeatMsg heartbeat_msg(&sender, 12345, 67890);
  HeartbeatAckMsg ack_msg(&sender, 12345, 67890, 99999);

  EXPECT_NE(state_msg.get_type_id(), state_ack_msg.get_type_id());
  EXPECT_NE(state_msg.get_type_id(), heartbeat_msg.get_type_id());
  EXPECT_NE(state_msg.get_type_id(), ack_msg.get_type_id());
  EXPECT_NE(state_ack_msg.get_type_id(), heartbeat_msg.get_type_id());
  EXPECT_NE(state_ack_msg.get_type_id(), ack_msg.get_type_id());
  EXPECT_NE(heartbeat_msg.get_type_id(), ack_msg.get_type_id());
}

// Test for Msg wrapper
TEST(MsgTest, CreateAndCast)
{
  MockTask sender;
  uint8_t state = 2;
  Msg msg = Msg::create<StateMsg>(&sender, state);

  EXPECT_EQ(msg.get_type_id(), &StateMsg::type_id);
  EXPECT_EQ(msg.get_priority(), 950);
  EXPECT_EQ(msg.get_sender(), &sender);

  const StateMsg* state_msg = msg.as<StateMsg>();
  ASSERT_NE(state_msg, nullptr);
  EXPECT_EQ(*state_msg->get_data(), state);

  const HeartbeatMsg* heartbeat_msg = msg.as<HeartbeatMsg>();
  EXPECT_EQ(heartbeat_msg, nullptr);
}

// Test for invalid cast
TEST(MsgTest, InvalidCast)
{
  MockTask sender;
  Msg msg = Msg::create<StateMsg>(&sender, 1);
  HeartbeatMsg* heartbeat_msg = msg.as<HeartbeatMsg>();
  EXPECT_EQ(heartbeat_msg, nullptr);
}

// Test cloning and copying messages
TEST(MsgTest, CloneAndCopy)
{
  MockTask sender;
  uint8_t state = 5;
  
  // Create original message
  Msg original = Msg::create<StateMsg>(&sender, state);
  
  // Test copy construction
  Msg copy1(original);
  EXPECT_EQ(copy1.get_priority(), original.get_priority());
  const StateMsg* state_msg1 = copy1.as<StateMsg>();
  ASSERT_NE(state_msg1, nullptr);
  EXPECT_EQ(*state_msg1->get_data(), state);

  // Test copy assignment
  Msg copy2 = Msg::create<HeartbeatMsg>(&sender, 0, 0);  // Different type
  copy2 = original;
  EXPECT_EQ(copy2.get_priority(), original.get_priority());
  const StateMsg* state_msg2 = copy2.as<StateMsg>();
  ASSERT_NE(state_msg2, nullptr);
  EXPECT_EQ(*state_msg2->get_data(), state);
  
  // Verify copies are independent
  StateMsg* mutable_state = original.as<StateMsg>();
  *const_cast<uint8_t*>(mutable_state->get_data()) = 10;
  EXPECT_EQ(*state_msg1->get_data(), state);  // copy1 unchanged
  EXPECT_EQ(*state_msg2->get_data(), state);  // copy2 unchanged
}
