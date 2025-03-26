/**
 * @file msg.h
 * @brief Defines the Msg class and related components for inter-task communication.
 *
 * This file provides the implementation of a messaging system that allows tasks to
 * exchange messages with different types, priorities, and payloads. It includes
 * the `BaseMsg` class for generic messages, the `TypedMsg` template for typed messages,
 * and the `Msg` wrapper class for managing message instances.
 */

#pragma once

#include <memory>
#include <string>
#include <utility>

namespace task
{
  class Task; // Forward declaration
}

namespace msg
{
  /**
   * @enum Type
   * @brief Defines the types of messages that can be sent between tasks.
   *
   * This enumeration represents the different categories of messages that can be exchanged
   * between tasks. Each type corresponds to a specific purpose or action.
   */
  enum class Type
  {
    STATE,         ///< Represents a request to change or report a state.
    STATE_ACK,     ///< Acknowledgment of a state change request.
    HEARTBEAT,     ///< A periodic message to indicate that a task is alive.
    HEARTBEAT_ACK, ///< Acknowledgment of a heartbeat message.
  };

  /**
   * @brief Converts a `Type` enum value to its string representation.
   * @param type The message type to convert.
   * @return A string representation of the message type.
   *
   * This utility function is useful for logging or debugging purposes, allowing users
   * to easily interpret the type of a message in human-readable form.
   */
  static std::string msg_type_to_string(Type type)
  {
    switch (type)
    {
      case Type::STATE:
        return "STATE";
      case Type::STATE_ACK:
        return "STATE_ACK";
      case Type::HEARTBEAT:
        return "HEARTBEAT";
      case Type::HEARTBEAT_ACK:
        return "HEARTBEAT_ACK";
      default:
        return "UNKNOWN";
    }
  }

  /**
   * @enum Priority
   * @brief Defines message priority levels, which determine processing order in the queue.
   *
   * This enumeration specifies the priority levels for messages. Messages with higher
   * priority are processed before those with lower priority. The order of the enumerations
   * is significant, as it determines the relative priority.
   */
  enum class Priority
  {
    LOW_PRIORITY,                   ///< Low-priority messages.
    MEDIUM_PRIORITY,                ///< Medium-priority messages.
    HIGH_PRIORITY,                  ///< High-priority messages that need urgent handling.
    HEARTBEAT_PRIORITY,             ///< Highest priority for heartbeat messages.
    HEARTBEAT_ACK_PRIORITY,         ///< Acknowledgment messages for heartbeats.
    STATE_ACK_PRIORITY,             ///< Acknowledgment messages should be processed timely
    STATE_TRANSITION_PRIORITY       ///< Highest priority for state transitions.
  };

  /**
   * @class BaseMsg
   * @brief Represents a generic message with a type, priority, and sender.
   *
   * This is the base class for all messages. It provides common functionality such as
   * retrieving the message type, priority, and sender. Derived classes can extend this
   * to include specific data payloads.
   */
  class BaseMsg
  {
  public:
    /**
     * @brief Constructs a BaseMsg instance.
     * @param type The type of the message.
     * @param priority The priority of the message.
     * @param sender The task that sent the message.
     *
     * This constructor initializes the basic properties of a message, including its type,
     * priority, and the task that sent it.
     */
    BaseMsg(Type type, Priority priority, task::Task* sender) :
      msg_type(type), msg_priority(priority), sending_task(sender) { }

    /**
     * @brief Gets the type of the message.
     * @return The message type.
     */
    Type get_type() const { return msg_type; }

    /**
     * @brief Gets the priority of the message.
     * @return The message priority.
     */
    Priority get_priority() const { return msg_priority; }

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
     * @brief Compares the priority of this message with another message.
     * @param other The other message to compare with.
     * @return True if this message has lower priority than the other.
     */
    bool operator<(const BaseMsg& other) const { return msg_priority < other.msg_priority; }

  protected:
    Type msg_type;               ///< The type of the message.
    Priority msg_priority;       ///< The priority of the message.
    task::Task* sending_task;    ///< The task that sent the message.
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
     * @brief Constructs a TypedMsg instance.
     * @param type The type of the message.
     * @param priority The priority of the message.
     * @param sender The task that sent the message.
     * @param data The data payload of the message.
     *
     * This constructor initializes the message with its type, priority, sender, and data payload.
     */
    TypedMsg(Type type, Priority priority, task::Task* sender, const DataT& data) :
      BaseMsg(type, priority, sender), msg_data(data) { }

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
  };

  /**
   * @class StateMsg
   * @brief Represents a message for state transitions.
   *
   * This message type is used to request state transitions between tasks.
   * It carries a single byte of data that represents the new state.
   */
  class StateMsg : public TypedMsg<StateMsg, uint8_t>
  {
  public:
    StateMsg(task::Task* sender, uint8_t state) :
      TypedMsg(Type::STATE, Priority::STATE_TRANSITION_PRIORITY, sender, { state }) { }
  };

  /**
   * @class StateAckMsg
   * @brief Represents an acknowledgment message for state transitions.
   *
   * This message type is used to acknowledge state transitions between tasks.
   * It carries a single byte of data that represents the new state.
   */
  class StateAckMsg : public TypedMsg<StateAckMsg, uint8_t>
  {
  public:
    StateAckMsg(task::Task* sender, uint8_t state) :
      TypedMsg(Type::STATE_ACK, Priority::STATE_ACK_PRIORITY, sender, { state }) { }
  };

  /**
   * @struct HeartbeatData
   * @brief Represents the data payload for a heartbeat message.
   *
   * This structure defines the data payload for a heartbeat message. It includes
   * a unique identifier and a timestamp to indicate the time of the heartbeat.
   */
  struct HeartbeatData
  {
    uint32_t unique_id; ///< A unique identifier for the heartbeat message.
    uint32_t timestamp; ///< The timestamp of the heartbeat message.
  };

  /**
   * @class HeartbeatMsg
   * @brief Represents a periodic heartbeat message.
   *
   * This message type is used to indicate that a task is alive and functioning.
   * It carries a unique identifier and a timestamp to track the heartbeat.
   */
  class HeartbeatMsg : public TypedMsg<HeartbeatMsg, HeartbeatData>
  {
  public:
    HeartbeatMsg(task::Task* sender, uint32_t unique_id, uint32_t timestamp) :
      TypedMsg(Type::HEARTBEAT, Priority::HEARTBEAT_PRIORITY, sender, { unique_id, timestamp }) { }

    uint32_t get_unique_id() const { return get_data()->unique_id; }
    uint32_t get_timestamp() const { return get_data()->timestamp; }
  };

  struct HeartbeatAckData
  {
    uint32_t orig_unique_id; ///< The unique identifier of the original heartbeat message.
    uint32_t orig_timestamp; ///< The timestamp of the original heartbeat message.
    uint32_t ack_timestamp;  ///< The timestamp of the acknowledgment message.
    // TODO: Identify need for any other fields
  };

  /**
   * @class HeartbeatAckMsg
   * @brief Represents an acknowledgment message for a heartbeat.
   *
   * This message type is used to acknowledge a heartbeat message from another task.
   * It carries the unique identifier and timestamp of the original heartbeat message.
   * Along with a timestamp to indicate the time of the acknowledgment.
   */
  class HeartbeatAckMsg : public TypedMsg<HeartbeatAckMsg, HeartbeatAckData>
  {
  public:
    HeartbeatAckMsg(task::Task* sender, uint32_t orig_unique_id, uint32_t orig_timestamp, uint32_t ack_timestamp) :
      TypedMsg(Type::HEARTBEAT_ACK, Priority::HEARTBEAT_ACK_PRIORITY, sender, { orig_unique_id, orig_timestamp, ack_timestamp }) { }

    uint32_t get_orig_unique_id() const { return get_data()->orig_unique_id; }
    uint32_t get_orig_timestamp() const { return get_data()->orig_timestamp; }
    uint32_t get_ack_timestamp() const { return get_data()->ack_timestamp; }
  };

  /**
   * @class Msg
   * @brief A wrapper class for managing message instances.
   *
   * This class provides a unified interface for working with messages of different types.
   * It uses a unique pointer to manage the lifetime of the underlying `BaseMsg` instance.
   * Containers for messages should use this class to store and manage message instances.
   */
  class Msg
  {
  public:
    /**
     * @brief Creates a new message instance of a specific type.
     * @tparam MsgT The message type to create.
     * @tparam Args The argument types for the message constructor.
     * @param args The arguments to pass to the message constructor.
     * @return A new message instance.
     *
     * This static method simplifies the creation of messages by forwarding the provided
     * arguments to the constructor of the specified message type.
     */
    template <typename MsgT, typename... Args>
    static Msg create(Args&&... args)
    {
      return Msg(std::make_unique<MsgT>(std::forward<Args>(args)...));
    }

    /**
     * @brief Get the type object
     * 
     * @return Type 
     */
    Type get_type() const { return msg_ptr->get_type(); }

    /**
     * @brief Get the priority object
     * 
     * @return Priority 
     */
    Priority get_priority() const { return msg_ptr->get_priority(); }

    /**
     * @brief Get the sender object
     * 
     * @return task::Task* 
     */
    task::Task* get_sender() const { return msg_ptr->get_sender(); }

    /**
     * @brief Compares the priority of this message with another message.
     * @param other The other message to compare with.
     * @return True if this message has lower priority than the other.
     */
    bool operator<(const Msg& other) const { return msg_ptr->get_priority() < other.msg_ptr->get_priority(); }

    /**
     * @brief Casts the message to a specific type (const version).
     * @tparam MsgT The message type to cast to.
     * @return A pointer to the message of the specified type, or nullptr if the cast fails.
     *
     * This method attempts to cast the underlying message to the specified type. If the
     * type of the message does not match the requested type, it returns `nullptr`.
     * This is useful for safely accessing the data of a specific message type.
     */
    template <typename MsgT>
    const MsgT* as() const
    {
      if (msg_ptr->get_type_id() == &MsgT::type_id)
      {
        return static_cast<const MsgT*>(msg_ptr.get());
      }
      return nullptr;
    }

    /**
     * @brief Casts the message to a specific type (non-const version).
     * @tparam MsgT The message type to cast to.
     * @return A pointer to the message of the specified type, or nullptr if the cast fails.
     *
     * This method is similar to the const version but allows modification of the underlying
     * message if the cast is successful. It is useful for scenarios where the message data
     * needs to be updated or modified.
     */
    template <typename MsgT>
    MsgT* as()
    {
      if (msg_ptr->get_type_id() == &MsgT::type_id)
      {
        return static_cast<MsgT*>(msg_ptr.get());
      }
      return nullptr;
    }

  private:
    /**
     * @brief A unique pointer to the underlying `BaseMsg` instance.
     *
     * This member variable manages the lifetime of the message object. It ensures that
     * the memory for the message is automatically released when the `Msg` instance is
     * destroyed. The `BaseMsg` pointer allows polymorphic behavior, enabling the `Msg`
     * class to handle messages of different types.
     */
    explicit Msg(std::unique_ptr<BaseMsg> ptr) : msg_ptr(std::move(ptr)) { }
    std::unique_ptr<BaseMsg> msg_ptr;
  };
}
