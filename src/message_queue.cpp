/**
 * @file message_queue.cpp
 * @brief Thread-safe message queue implementation
 *
 */

#include <algorithm>

#include "message_queue.h"

MessageQueue::MessageQueue(std::size_t msg_size, std::size_t max_msgs) : pool(msg_size, max_msgs)
{
  // Reserve the vector's capacity to match the maximum number of messages.
  queue.reserve(max_msgs);
}

MessageQueue::~MessageQueue()
{
  std::lock_guard<std::mutex> lock(queue_mutex);
  // Drain any remaining nodes, call destructors, and return memory to the pool.
  for (Node* node : queue)
  {
    node->~Node();
    pool.deallocate(node);
  }
  queue.clear();
}

void MessageQueue::enqueue(const msg::Msg &msg)
{
  // Allocate memory for a Node from the memory pool.
  void* raw = pool.allocate();
  assert(raw != nullptr && "Memory pool out of memory!");

  // Construct a Node in the allocated memory using placement new.
  {
    std::lock_guard<std::mutex> lock(queue_mutex);
    Node* node = new (raw) Node{ msg, sequence++ };
    // Insert the new node pointer into our fixed capacity container.
    queue.push_back(node);
    std::push_heap(queue.begin(), queue.end(), NodeComparator());
  }
  queue_condition.notify_one();
}

std::optional<msg::Msg> MessageQueue::dequeue()
{
  std::unique_lock<std::mutex> lock(queue_mutex);
  queue_condition.wait(lock, [this]() { return !queue.empty() || shutdown; });

  if (shutdown && queue.empty())
  {
    sequence = 0;
    return std::nullopt;
  }

  // Reorder the heap and pop the top element.
  std::pop_heap(queue.begin(), queue.end(), NodeComparator());
  Node* node = queue.back();
  queue.pop_back();

  // If the queue is empty, reset the sequence number to 0.
  if (queue.empty())
  {
    sequence = 0;
  }

  msg::Msg result = node->message;
  node->~Node();
  pool.deallocate(node);

  return result;
}

std::optional<msg::Msg> MessageQueue::try_dequeue(std::chrono::milliseconds timeout)
{
  std::unique_lock<std::mutex> lock(queue_mutex);
  if (!queue_condition.wait_for(lock, timeout, [this]() { return !queue.empty() || shutdown; }))
  {
    return std::nullopt; // No message available
  }

  std::pop_heap(queue.begin(), queue.end(), NodeComparator());
  Node* node = queue.back();
  queue.pop_back();
  
  msg::Msg result = node->message;
  node->~Node();
  pool.deallocate(node);

  return result;
}

bool MessageQueue::is_empty() const
{
  std::lock_guard<std::mutex> lock(queue_mutex);
  return queue.empty();
}

void MessageQueue::notify_shutdown()
{
  std::lock_guard<std::mutex> lock(queue_mutex);
  shutdown = true;
  queue_condition.notify_all();
}
