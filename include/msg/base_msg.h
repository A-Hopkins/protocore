/**
 * @file base_msg.h
 * @brief Defines the BaseMsg class and related components for inter-task communication.
 *
 * This file provides the implementation of a messaging system that allows tasks to
 * exchange messages with different types, priorities, and payloads. It includes
 * the `BaseMsg` class for generic messages, the `TypedMsg` template for typed messages.
 */

#pragma once

#include <memory>
#include <cstdint>

namespace task
{
  class Task; // Forward declaration
}

namespace msg
{
  /**
   * @class BaseMsg
   * @brief Represents a generic message with a priority and sender.
   *
   * This is the base class for all messages. It provides common functionality such as
   * retrieving the message priority and sender. Derived classes can extend this
   * to include specific data payloads.
   */
  class BaseMsg
  {
  public:

    /**
     * @brief Destroys the BaseMsg instance.
     *
     * This virtual destructor ensures that the destructors of derived classes are called
     * when deleting a `BaseMsg` pointer. It is necessary for proper cleanup of resources.
     */
    virtual ~BaseMsg() = default;

    /**
     * @brief Gets the priority of the message.
     * @return The message priority.
     */
    uint16_t get_priority() const { return msg_priority; }

    /**
     * @brief Gets the sender of the message.
     * @return A pointer to the sending task.
     */
    task::Task* get_sender() const { return sending_task; }

    /**
     * @brief Gets a unique identifier for the message type.
     * @return A pointer to the type identifier.
     */
    virtual const void* get_type_id() const = 0;

    /**
     * @brief Creates a clone of this message.
     * @return A unique pointer to a new copy of this message.
     *
     * This virtual method allows polymorphic cloning of message objects.
     * Each derived class must implement this to create a proper deep copy.
     */
    virtual std::unique_ptr<BaseMsg> clone() const = 0;

    /**
     * @brief Compares the priority of this message with another message.
     * @param other The other message to compare with.
     * @return True if this message has lower priority than the other.
     */
    bool operator<(const BaseMsg& other) const { return msg_priority < other.msg_priority; }

  protected:
    task::Task* sending_task; ///< The task that sent the message.

    /**
     * @brief The priority level for this message type.
     * Each derived class must define its own priority value.
     */
    uint16_t msg_priority;

    /**
     * @brief Constructs a BaseMsg instance.
     * @param sender The task that sent the message.
     * @param priority The priority of the message.
     * 
     * This constructor initializes the basic properties of a message. The priority should be
     * set by the dervied class to indicate the importance of the message. Higher priority is
     * a higher value.
     */
    BaseMsg(task::Task* sender, uint16_t priority = 0): sending_task(sender), msg_priority(priority) { }
  };

  /**
   * @class TypedMsg
   * @brief Represents a typed message with a specific data payload.
   *
   * This template class extends `BaseMsg` to include a data payload of a specific type.
   * It is designed for messages that carry structured data. Users can define their own
   * message types by deriving from this class and specifying the data type and size.
   *
   * @tparam Derived The derived class type.
   * @tparam DataT The type of the data payload.
   */
  template <typename Derived, typename DataT>
  class TypedMsg : public BaseMsg
  {
  public:
    /**
     * @brief A static identifier for the message type.
     *
     * This identifier is used to distinguish between different message types at runtime.
     * The the address of that static member is unique for each different derived type and
     * so you're comparing the addresses (not the integer value) of these static members.
     * You do not need to change the type_id constexpr for each derived class.
     */
    static constexpr uint16_t type_id = 0;

    /**
     * @brief Creates a clone of this message.
     * @return A unique pointer to a new copy of this message.
     *
     * This implementation creates a new instance of the derived message type
     * with the same data as this instance.
     */
    std::unique_ptr<BaseMsg> clone() const override
    {
      return std::make_unique<Derived>(static_cast<const Derived&>(*this));
    }

    /**
     * @brief Gets a pointer to the data payload.
     * @return A pointer to the data array.
     */
    const DataT* get_data() const { return &msg_data; }

    /**
     * @brief Gets the size of the data payload.
     * @return The size of the data type.
     */
    size_t get_data_size() const { return sizeof(msg_data); }

    /**
     * @brief Gets a unique identifier for the message type.
     * @return A pointer to the type identifier.
     */
    const void* get_type_id() const override { return &Derived::type_id; }

  protected:
    DataT msg_data; ///< The data payload of the message.

    /**
     * @brief Constructs a TypedMsg instance.
     * @param sender The task that sent the message.
     * @param data The data payload of the message.
     * @param priority The priority of the message.
     * 
     * This constructor initializes the message with its sender, and data payload.
     */
    TypedMsg(task::Task* sender, const DataT& data, uint16_t priority = 0) :
      BaseMsg(sender, priority), msg_data(data) { }
  };
}
