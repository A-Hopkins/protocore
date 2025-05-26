/**
 * @file msg.h
 * @brief Defines the Msg class
 *
 * This file provides the implementation of the `Msg` class, which is a generic
 * message that has variant data in it.
 */
#pragma once
#include "msg_traits.h"
#include <cassert>

#ifdef USE_CUSTOM_MSG_VARIANT
#include "msg_variant_types.h"
#else
#include "msg_variant_types_default.h"
#endif

namespace task
{
  class Task; // Forward declaration
}

namespace msg
{
  /**
   * @class Msg
   * @brief Represents a message with a specific type and priority.
   *
   * This class is used to encapsulate messages that are sent between tasks.
   * It provides methods to access the message data, sender, priority, and type.
   */
  class Msg
  {
  public:
    /**
     * @brief Construct a new Msg
     *
     * @param sender Pointer to the task that sent the message
     * @param data The message data of type MessageVariant
     */
    Msg(task::Task* sender, const MessageVariant& data) : sending_task(sender), msg_data(data)
    {
      msg_priority = get_internal_priority();
      msg_type     = get_internal_type();
    }

    /**
     * @brief Checks if the stored data is of a specific type.
     *
     * @tparam T The type to check against
     * @return true if the stored data matches the specified type, false otherwise
     */
    template <typename T>
    bool has_data_type() const
    {
      return std::holds_alternative<T>(msg_data);
    }

    /**
     * @brief Retrieve the data of the message as a specific type.
     *
     * @tparam T The expected type of the data of MessageVariant
     * @return const T* A pointer to the data if it matches the type, nullptr otherwise
     */
    template <typename T>
    const T* get_data_as() const
    {
      return std::get_if<T>(&msg_data);
    }

    /**
     * @brief Get the priority level of the message
     *
     * @return Priority
     */
    Priority get_priority() const { return msg_priority; }

    /**
     * @brief Get the type of the message
     *
     * @return Type
     */
    Type get_type() const { return msg_type; }

    /**
     * @brief Get the task that sent the message
     *
     * @return Pointer to the sender task
     */
    task::Task* get_sender() const { return sending_task; }

    /**
     * @brief Defines comparison for priority ordering
     *
     * @param other The other message to compare against
     * @return true if this message has a lower priority than the other
     */
    bool operator<(const Msg& other) const { return msg_priority < other.msg_priority; }

    /**
     * @brief Converts the message to a string for logging
     * @return The message formatted as a string
     */
    std::string str() const
    {
      return std::visit(
          [](const auto& actual_msg) -> std::string
          {
            using T = std::decay_t<decltype(actual_msg)>;

            static_assert(msg::has_str<T>, "All message types used in MessageVariant must "
                                                   "implement std::string str() const");

            return actual_msg.str();
          },
          msg_data);
    }

  private:
    task::Task*    sending_task; ///< The task that sent the message
    MessageVariant msg_data;     ///< The message data
    Priority       msg_priority; ///< The priority of the message
    Type           msg_type;     ///< The type of the message

    /**
     * @brief Get the internal priority from the MessageVariant
     *
     * @return Priority
     */
    Priority get_internal_priority() const
    {
      // The index of the currently held alternative in the variant.
      std::size_t index = msg_data.index();
      // You might want to assert the index is valid relative to message_priorities
      assert(index < (sizeof(message_priorities) / sizeof(message_priorities[0])));
      return message_priorities[index];
    }

    /**
     * @brief Get the internal type from the MessageVariant
     *
     * @return Type
     */
    Type get_internal_type() const
    {
      std::size_t index = msg_data.index();
      assert(index < std::variant_size<MessageVariant>::value);
      return static_cast<Type>(msg_data.index());
    }
  };
} // namespace msg
