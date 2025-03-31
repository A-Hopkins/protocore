/**
 * @file message_queue.h
 * @brief Thread-safe message queue for inter-task communication.
 *
 */
#pragma once

#include <cassert>
#include <cstddef>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

#include "memory_pool.h"
#include "msg/msg.h"

/**
 * @brief Thread-safe IPC message queue.
 * 
 * This queue stores messages of type msg::Msg (which provides type erasure for message types).
 * Each MessageQueue uses its own MemoryPool for allocating nodes in which the messages are stored.
 * The queue is priority-based (using msg::Msg::operator<) so that higher priority messages are dequeued first.
 * Blocking dequeue, try_dequeue with timeout, and assertions for runtime safety are provided. * 
 */
class MessageQueue
{
public:
  /**
   * @brief Construct a new Message Queue object
   * 
   * @param msg_size Size of each message in bytes, size in bytes for each memory block.
   * @param max_msgs Maximum number of messages in the queue (The number of storage blocks to preallocate)
   */
  MessageQueue(std::size_t msg_size, std::size_t max_msgs);

  /**
   * @brief Destroy the Message Queue object
   * 
   * Deallocates all messages in the queue and frees the memory pool.
   */
  ~MessageQueue();

  /**
   * @brief Enqueue a message.
   *
   * The message is deep-copied (by invoking msg::Msg's copy constructor) into a node allocated from the memory pool.
   *
   * @param msg The message to enqueue.
   */
  void enqueue(const msg::Msg &msg);

  /**
   * @brief Dequeue a message (blocking).
   *
   * If the queue is empty, this call blocks until a message is available.
   *
   * @return msg::Msg A message from the queue.
   */
  msg::Msg dequeue();

  /**
   * @brief Attempt to dequeue a message within a given timeout.
   *
   * This method attempts to retrieve a message from the queue. If a message is available
   * or becomes available within the timeout period, it returns an optional containing the message.
   * If no message is available within the timeout, it returns an empty optional.
   *
   *
   * @param timeout Maximum time to wait, default is 0 (non-blocking) (in milliseconds)
   * @return std::optional<msg::Msg> Contains the dequeued message if successful, empty otherwise
   */
  std::optional<msg::Msg> try_dequeue(std::chrono::milliseconds timeout = std::chrono::milliseconds(0));

  /**
   * @brief Check whether the queue is empty.
   *
   * @return true if the queue is empty, false otherwise.
   */
  bool is_empty() const;

private:
  // Disable copying.
  MessageQueue(const MessageQueue&) = delete;
  MessageQueue& operator=(const MessageQueue&) = delete;

  /**
   * @brief A local node that stores a message.
   */
  struct Node
  {
    msg::Msg message;
    uint64_t sequence_number;
  };

  /**
   * @brief Comparator for priority ordering.
   *
   * When using std::push_heap/std::pop_heap, this comparator determines the order based on msg::Msg::operator<.
   */
  struct NodeComparator
  {
    bool operator()(const Node* a, const Node* b) const
    {
      if (a->message < b->message)
      {
        return true;
      }
        
      if (b->message < a->message)
      {
        return false;
      }
      // Same priority, use sequence number for FIFO ordering
      return a->sequence_number > b->sequence_number;
    }
  };

  MemoryPool pool; ///< Memory pool for allocating nodes.
  std::vector<Node*> queue; ///< Vector of pointers to nodes in the queue.
  mutable std::mutex queue_mutex; ///< Mutex for thread safety.
  std::condition_variable queue_condition; ///< Condition variable for blocking dequeue.
  uint64_t sequence = 0; ///< Sequence number for FIFO ordering of same-priority messages.
};
