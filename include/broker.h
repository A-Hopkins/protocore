/**
 * @file broker.h
 * @brief Provides message brokering and subscription management for tasks.
 *
 * This file declares the Broker class, which manages subscriptions for various
 * message types and routes incoming messages to subscribed tasks. Tasks can
 * efficiently communicate by publishing messages of specific types, ensuring
 * loosely coupled and modular design.
 */

#pragma once

#include "msg/msg.h"
#include "task.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <mutex>
#include <vector>

/**
 * @class Broker
 * @brief A class that manages message passing between tasks.
 *
 * The Broker class provides a centralized message broker that allows tasks to communicate with each
 * other via message passing. It maintains a global list of subscribers for each message type and
 * routes messages to the appropriate tasks based on their subscriptions.
 */
class Broker
{
public:
  // We can determine message type count from the MessageVariant
  static constexpr size_t MESSAGE_TYPE_COUNT = std::variant_size_v<msg::MessageVariant>;

  // Default max subscribers per type
  static constexpr size_t DEFAULT_MAX_SUBSCRIBERS = 16;

  /**
   * @brief Initialize the broker with specified capacity
   * @param max_subscribers Maximum subscribers per message type
   */
  static void initialize(size_t max_subscribers = DEFAULT_MAX_SUBSCRIBERS)
  {
    std::lock_guard<std::mutex> lock(sub_mutex);

    // Initialize vectors for each message type with reserved capacity
    for (size_t i = 0; i < MESSAGE_TYPE_COUNT; ++i)
    {
      subscriber_lists[i].reserve(max_subscribers);
    }

    initialized = true;
  }

  /**
   * @brief Publishes a message to all subscribers of that message type.
   * @param msg The message to be published.
   */
  static inline void publish(const msg::Msg& msg)
  {
    std::lock_guard<std::mutex> lock(sub_mutex);
    assert(initialized && "Broker not initialized!");

    auto type_index = static_cast<size_t>(msg.get_type());
    assert(type_index < MESSAGE_TYPE_COUNT && "Invalid message type!");

    // Iterate over the vector of weak pointers.
    // Lock each pointer. If expired, remove it.
    auto& subscribers = subscriber_lists[type_index];
    for (auto it = subscribers.begin(); it != subscribers.end();)
    {
      if (auto subscriber = it->lock())
      {
        subscriber->deliver_message(msg);
        ++it;
      }
      else
      {
        // Erase expired entry.
        it = subscribers.erase(it);
      }
    }
  }

  /**
   * @brief Subscribes a task to receive messages of a specific type.
   * @param task Pointer to the task that should receive the message.
   * @param type The type of message to subscribe to.
   */
  static inline void subscribe(std::shared_ptr<task::Task> task, msg::Type type)
  {
    std::lock_guard<std::mutex> lock(sub_mutex);
    assert(initialized && "Broker not initialized!");

    auto type_index = static_cast<size_t>(type);
    assert(type_index < MESSAGE_TYPE_COUNT && "Invalid message type!");

    auto& subscribers = subscriber_lists[type_index];
    // Prevent duplicate subscriptions
    auto found = std::find_if(subscribers.begin(), subscribers.end(),
                              [&task](const std::weak_ptr<task::Task>& wp)
                              {
                                auto shared = wp.lock();
                                return shared && shared.get() == task.get();
                              });
    if (found == subscribers.end())
    {
      subscribers.push_back(task);
    }
  }

private:
  static inline std::mutex sub_mutex; ///< Mutex for synchronizing access to global subscribers.
  static inline bool       initialized = false; ///< Flag to check if the broker is initialized.

  /**
   * @brief A fixed-size array that maintains lists of tasks subscribed to each message type.
   *
   * This static member variable enables efficient task communication via message passing. The array
   * has exactly MESSAGE_TYPE_COUNT elements (one for each message type defined in msg::Type), where
   * each element is a vector of task::Task* pointers representing the subscribers for that message
   * type.
   *
   * The array is indexed directly by casting the msg::Type enum to size_t, providing O(1) lookup
   * performance. When a task publishes a message using publish(), all tasks that have subscribed to
   * that message type via subscribe() will receive the message in their message queue.
   *
   * Memory for subscriber lists is pre-allocated during initialization to avoid runtime allocations
   * during operation. Since the array is accessed concurrently by multiple tasks, access is
   * protected by sub_mutex to ensure thread safety.
   */
  static inline std::array<std::vector<std::weak_ptr<task::Task>>, MESSAGE_TYPE_COUNT>
      subscriber_lists;
};
