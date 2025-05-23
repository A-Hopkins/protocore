#include "msg/msg.h"
#include "msg/system_msgs.h"
#include <gtest/gtest.h>
#include <queue>

using namespace msg;

// Minimal declaration for task::Task to satisfy Msg constructor requirements.
namespace task
{
  class Task
  {
  public:
    Task(const std::string& name) : name(name) {}
    const std::string& get_name() const { return name; }

  protected:
    std::string name;
  };
} // namespace task

// Minimal MockTask for testing, inheriting from task::Task
class MockTask : public task::Task
{
public:
  MockTask(const std::string& name) : task::Task(name) {}
};

// Test for Msg priority ordering
TEST(MsgPriorityTest, PriorityOrdering)
{
  MockTask     sender("Sender");
  StateMsg     high_state_msg{1};
  HeartbeatMsg low_heartbeat_msg{12345, 67890};
  Msg          high_priority_msg(&sender, high_state_msg);
  Msg          low_priority_msg(&sender, low_heartbeat_msg);

  std::priority_queue<Msg> queue;
  queue.push(std::move(low_priority_msg));
  queue.push(std::move(high_priority_msg));

  ASSERT_EQ(queue.size(), 2);
  EXPECT_EQ(queue.top().get_priority(), 100); // StateMsg priority is 100
  queue.pop();
  EXPECT_EQ(queue.top().get_priority(), 50); // HeartbeatMsg priority is 50
}

// Test for StateMsg creation and data access
TEST(StateMsgTest, ConstructorAndDataAccess)
{
  MockTask sender("Sender");
  uint8_t  state = 1;
  StateMsg state_msg{state};
  Msg      msg(&sender, state_msg);

  EXPECT_EQ(msg.get_priority(), 100);
  EXPECT_EQ(msg.get_sender(), &sender);

  const StateMsg* ptr = msg.get_data_as<StateMsg>();
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(ptr->state, state);
}

// Test for HeartbeatMsg creation and data access
TEST(HeartbeatMsgTest, ConstructorAndDataAccess)
{
  MockTask     sender("Sender");
  uint32_t     unique_id = 12345;
  uint64_t     timestamp = 67890;
  HeartbeatMsg heartbeat_msg{unique_id, timestamp};
  Msg          msg(&sender, heartbeat_msg);

  EXPECT_EQ(msg.get_priority(), 50);
  EXPECT_EQ(msg.get_sender(), &sender);

  const HeartbeatMsg* ptr = msg.get_data_as<HeartbeatMsg>();
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(ptr->unique_id, unique_id);
  EXPECT_EQ(ptr->timestamp, timestamp);
}

// Test for HeartbeatAckMsg creation and data access
TEST(HeartbeatAckMsgTest, AckMessage)
{
  MockTask     sender("Sender");
  uint32_t     unique_id = 54321;
  uint64_t     timestamp = 98765;
  HeartbeatMsg heartbeat_msg{unique_id, timestamp};

  uint64_t        ack_timestamp = 99999;
  HeartbeatAckMsg ack_msg{unique_id, timestamp, ack_timestamp};
  Msg             msg(&sender, ack_msg);

  const HeartbeatAckMsg* ptr = msg.get_data_as<HeartbeatAckMsg>();
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(ptr->orig_unique_id, unique_id);
  EXPECT_EQ(ptr->orig_timestamp, timestamp);
  EXPECT_EQ(ptr->ack_timestamp, ack_timestamp);
}

// Test for unique type identifiers (using get_type)
TEST(MsgUniqueIdsTest, UniqueTypeIds)
{
  MockTask        sender("Sender");
  StateMsg        state_msg{1};
  StateAckMsg     state_ack_msg{2};
  HeartbeatMsg    heartbeat_msg{12345, 67890};
  HeartbeatAckMsg ack_msg{12345, 67890, 99999};

  Msg msg1(&sender, state_msg);
  Msg msg2(&sender, state_ack_msg);
  Msg msg3(&sender, heartbeat_msg);
  Msg msg4(&sender, ack_msg);

  EXPECT_NE(msg1.get_type(), msg2.get_type());
  EXPECT_NE(msg1.get_type(), msg3.get_type());
  EXPECT_NE(msg1.get_type(), msg4.get_type());
  EXPECT_NE(msg2.get_type(), msg3.get_type());
  EXPECT_NE(msg2.get_type(), msg4.get_type());
  EXPECT_NE(msg3.get_type(), msg4.get_type());
}

// Test for Msg wrapper create and cast (also verifies invalid casts)
TEST(MsgTest, CreateAndCast)
{
  MockTask sender("Sender");
  uint8_t  state = 2;
  StateMsg state_msg{state};
  Msg      msg(&sender, state_msg);

  EXPECT_EQ(msg.get_priority(), 100);
  EXPECT_EQ(msg.get_sender(), &sender);

  const StateMsg* state_ptr = msg.get_data_as<StateMsg>();
  ASSERT_NE(state_ptr, nullptr);
  EXPECT_EQ(state_ptr->state, state);

  const HeartbeatMsg* heartbeat_ptr = msg.get_data_as<HeartbeatMsg>();
  EXPECT_EQ(heartbeat_ptr, nullptr);
}

// New test to verify has_data_type functionality
TEST(MsgTest, HasDataTypeTest)
{
  MockTask sender("Sender");
  uint8_t  state = 3;
  StateMsg state_msg{state};
  Msg      msg(&sender, state_msg);

  EXPECT_TRUE(msg.has_data_type<StateMsg>());
  EXPECT_FALSE(msg.has_data_type<HeartbeatMsg>());
}
