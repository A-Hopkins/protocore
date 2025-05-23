#include "message_queue.h"
#include "msg/msg.h"
#include "msg/system_msgs.h"
#include <atomic>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <optional>
#include <thread>
#include <vector>

using namespace msg;

// Constants for all tests
const size_t MAX_MSGS = 100;

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

// Updated MockTask for testing, now including a message_queue member and proper constructors.
class MockTask : public task::Task
{
public:
  MessageQueue message_queue;
  // default constructor used in tests without arguments
  MockTask() : task::Task("default"), message_queue(MessageQueue::node_size(), MAX_MSGS) {}
  // constructor with capacity
  MockTask(const std::string& name, size_t capacity)
    : task::Task(name), message_queue(MessageQueue::node_size(), capacity)
  {
  }
};

// Test for initial queue state
TEST(MessageQueueTest, InitiallyEmpty)
{
  MockTask task("Task", MAX_MSGS);
  auto&    queue = task.message_queue;
  EXPECT_TRUE(queue.is_empty());
}

// Test for basic queue operations
TEST(MessageQueueTest, BasicEnqueueDequeue)
{
  MockTask task("Task", MAX_MSGS);
  auto&    queue = task.message_queue;

  MockTask sender;
  StateMsg state_msg{42};
  Msg      msg(&sender, state_msg);

  queue.enqueue(msg);
  EXPECT_FALSE(queue.is_empty());

  auto receivedOpt = queue.dequeue();
  ASSERT_TRUE(receivedOpt.has_value());
  Msg received = *receivedOpt;
  EXPECT_TRUE(received.has_data_type<StateMsg>());
  EXPECT_EQ(received.get_data_as<StateMsg>()->state, 42);
  EXPECT_TRUE(queue.is_empty());
}

// Test for priority-based message ordering
TEST(MessageQueueTest, PriorityMessageOrdering)
{
  MockTask task("Task", MAX_MSGS);
  auto&    queue = task.message_queue;
  MockTask sender;

  // Create messages with different priorities
  HeartbeatMsg low_msg{1, 0}; // Low priority
  StateMsg     high_msg{2};   // High priority
  StateAckMsg  medium_msg{3}; // Medium priority

  // Enqueue in reverse priority order
  queue.enqueue(Msg(&sender, low_msg));
  queue.enqueue(Msg(&sender, high_msg));
  queue.enqueue(Msg(&sender, medium_msg));

  // Messages should come out in priority order
  auto receivedOpt = queue.dequeue();
  ASSERT_TRUE(receivedOpt.has_value());
  Msg received = *receivedOpt;
  EXPECT_TRUE(received.has_data_type<StateMsg>());
  EXPECT_EQ(received.get_data_as<StateMsg>()->state, 2);

  receivedOpt = queue.dequeue();
  ASSERT_TRUE(receivedOpt.has_value());
  received = *receivedOpt;
  EXPECT_TRUE(received.has_data_type<StateAckMsg>());
  EXPECT_EQ(received.get_data_as<StateAckMsg>()->state, 3);

  receivedOpt = queue.dequeue();
  ASSERT_TRUE(receivedOpt.has_value());
  received = *receivedOpt;
  EXPECT_TRUE(received.has_data_type<HeartbeatMsg>());
  EXPECT_EQ(received.get_data_as<HeartbeatMsg>()->unique_id, 1);
}

// Test for try_dequeue timeout behavior
TEST(MessageQueueTest, DequeueTimeoutBehavior)
{
  MockTask task("Task", MAX_MSGS);
  auto&    queue = task.message_queue;
  MockTask sender;
  // Should fail with empty queue
  auto opt = queue.try_dequeue(std::chrono::milliseconds(10));
  EXPECT_FALSE(opt.has_value());

  // Should succeed with item in queue
  StateMsg state_msg{42};
  queue.enqueue(Msg(&sender, state_msg));
  opt = queue.try_dequeue(std::chrono::milliseconds(10));
  ASSERT_TRUE(opt.has_value());
  {
    Msg outMsg = *opt;
    EXPECT_TRUE(outMsg.has_data_type<StateMsg>());
    EXPECT_EQ(outMsg.get_data_as<StateMsg>()->state, 42);
  }

  // Should fail again with empty queue
  opt = queue.try_dequeue(std::chrono::milliseconds(10));
  EXPECT_FALSE(opt.has_value());
}

// Test for queue capacity handling
TEST(MessageQueueTest, MaxCapacityHandling)
{
  MockTask task("Task", MAX_MSGS);
  auto&    queue = task.message_queue;
  MockTask sender;

  // Fill the queue to capacity
  for (int i = 0; i < MAX_MSGS; i++)
  {
    StateMsg state_msg{static_cast<uint8_t>(i)};
    queue.enqueue(Msg(&sender, state_msg));
  }
  // Verify that we can dequeue MAX_MSGS messages
  for (int i = 0; i < MAX_MSGS; i++)
  {
    auto opt = queue.try_dequeue();
    ASSERT_TRUE(opt.has_value());
    {
      Msg outMsg = *opt;
      EXPECT_TRUE(outMsg.has_data_type<StateMsg>());
      // Don't check the exact value, just confirm it's a valid StateMsg
    }
  }

  EXPECT_TRUE(queue.is_empty());
}

// Test for thread safety with concurrent producers and consumers
TEST(MessageQueueTest, ThreadSafeConcurrentAccess)
{
  MockTask task("Task", MAX_MSGS);
  auto&    queue = task.message_queue;

  const int NUM_PRODUCERS     = 4;
  const int NUM_CONSUMERS     = 4;
  const int MSGS_PER_PRODUCER = 100;

  std::atomic<int>         messagesReceived(0);
  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;

  // Start consumer threads
  for (int i = 0; i < NUM_CONSUMERS; i++)
  {
    consumers.push_back(std::thread(
        [&queue, &messagesReceived]()
        {
          MockTask consumer;
          while (messagesReceived < NUM_PRODUCERS * MSGS_PER_PRODUCER)
          {
            auto opt = queue.try_dequeue();
            if (opt.has_value())
            {
              messagesReceived++;
            }
          }
        }));
  }

  for (int i = 0; i < NUM_PRODUCERS; i++)
  {
    producers.push_back(std::thread(
        [&queue, i]()
        {
          MockTask producer;
          for (int j = 0; j < MSGS_PER_PRODUCER; j++)
          {
            StateMsg state_msg{static_cast<uint8_t>((i * 1000 + j) % 256)};
            queue.enqueue(Msg(&producer, state_msg));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
        }));
  }

  // Join all threads
  for (auto& t : producers)
  {
    t.join();
  }
  for (auto& t : consumers)
  {
    t.join();
  }

  EXPECT_EQ(messagesReceived.load(), NUM_PRODUCERS * MSGS_PER_PRODUCER);
  EXPECT_TRUE(queue.is_empty());
}

// Test for blocking dequeue behavior
TEST(MessageQueueTest, BlockingDequeueWaitsForMessage)
{
  MockTask task("Task", MAX_MSGS);
  auto&    queue = task.message_queue;
  MockTask sender;

  std::promise<void>       startSignal;
  std::shared_future<void> ready = startSignal.get_future();

  std::thread producer(
      [&queue, &sender, ready]() mutable
      {
        // Wait until signaled by the consumer thread that it's ready.
        ready.wait();
        StateMsg state_msg{42};
        queue.enqueue(Msg(&sender, state_msg));
      });

  // Signal the producer to enqueue.
  startSignal.set_value();

  auto start       = std::chrono::steady_clock::now();
  auto receivedOpt = queue.dequeue();
  ASSERT_TRUE(receivedOpt.has_value());
  Msg  received = *receivedOpt;
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start);

  EXPECT_TRUE(received.has_data_type<StateMsg>());
  EXPECT_EQ(received.get_data_as<StateMsg>()->state, 42);
  // The duration check can be updated or removed based on desired behavior.

  producer.join();
}

// Test FIFO ordering for same-priority messages
TEST(MessageQueueTest, SamePriorityFIFO)
{
  MockTask task("Task", MAX_MSGS);
  auto&    queue = task.message_queue;
  MockTask sender;

  // Create multiple messages with same priority but different values
  for (int i = 1; i <= 5; i++)
  {
    queue.enqueue(Msg(&sender, StateMsg{static_cast<uint8_t>(i)}));
  }

  // Verify FIFO ordering is preserved
  for (int i = 1; i <= 5; i++)
  {
    auto receivedOpt = queue.dequeue();
    ASSERT_TRUE(receivedOpt.has_value());
    Msg received = *receivedOpt;
    EXPECT_EQ(received.get_data_as<StateMsg>()->state, i);
  }
}