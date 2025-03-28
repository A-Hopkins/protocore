/**
 * @file msg.h
 * @brief Defines the Msg class
 *
 * This file provides the implementation of the `Msg` class, which is a wrapper for
 * managing message instances. It includes methods for creating, copying, and casting
 * messages of different types. The `Msg` class uses a unique pointer to manage the
 * lifetime of the underlying `BaseMsg` instance.
 */
#pragma once
#include <utility>

#include "base_msg.h"

namespace msg
{
  /**
   * @class Msg
   * @brief A wrapper class for managing message instances.
   *
   * This class provides a unified interface for working with messages of different types.
   * It uses a unique pointer to manage the lifetime of the underlying `BaseMsg` instance.
   * Containers for messages should use this class to store and manage message instances.
   * 
   * A msg object is created on the heap using a unique pointer and to place it on a queue
   * the copy constructors are used to create a deep copy of the message. The copy is what
   * is added to the queues so that the original message can be cleaned up after the copy.
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
     * 
     * @example Msg msg = Msg::create<StateMsg>(&sender, state);
     */
    template <typename MsgT, typename... Args>
    static Msg create(Args&&... args)
    {
      return Msg(std::make_unique<MsgT>(std::forward<Args>(args)...));
    }

    /**
     * @brief Get the type identifier for this message
     * 
     * @return const void* Pointer to the type identifier
     */
    const void* get_type_id() const { return msg_ptr->get_type_id(); }

    /**
     * @brief Get the priority object
     * 
     * @return Priority 
     */
    uint16_t get_priority() const { return msg_ptr->get_priority(); }

    /**
     * @brief Get the sender object
     * 
     * @return task::Task* 
     */
    task::Task* get_sender() const { return msg_ptr->get_sender(); }

    /**
     * @brief A unique pointer to the underlying `BaseMsg` instance.
     *
     * This member variable manages the lifetime of the message object. It ensures that
     * the memory for the message is automatically released when the `Msg` instance is
     * destroyed. The `BaseMsg` pointer allows polymorphic behavior, enabling the `Msg`
     * class to handle messages of different types.
     */
    explicit Msg(std::unique_ptr<BaseMsg> ptr) : msg_ptr(std::move(ptr)) { }

    /**
     * @brief Copy constructor.
     * @param other The message to copy.
     *
     * Creates a new message that is a deep copy of the original.
     */
    Msg(const Msg& other) : msg_ptr(other.msg_ptr ? other.msg_ptr->clone() : nullptr) { }

    /**
     * @brief Copy assignment operator.
     * @param other The message to copy.
     * @return A reference to this message.
     *
     * Creates a deep copy of the original message.
     */
    Msg& operator=(const Msg& other)
    {
      if (this != &other) {
        msg_ptr = other.msg_ptr ? other.msg_ptr->clone() : nullptr;
      }
      return *this;
    }

    /**
     * @brief Move constructor.
     * @param other The message to move from.
     *
     * Transfers ownership of the message pointer.
     */
    Msg(Msg&& other) noexcept = default;

    /**
     * @brief Move assignment operator.
     * @param other The message to move from.
     * @return A reference to this message.
     *
     * Transfers ownership of the message pointer.
     */
    Msg& operator=(Msg&& other) noexcept = default;

    /**
     * @brief Compares the priority of this message with another message.
     * @param other The other message to compare with.
     * @return True if this message has lower priority than the other.
     */
    bool operator<(const Msg &other) const
    {
      return msg_ptr->get_priority() < other.msg_ptr->get_priority(); 
    }

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
    std::unique_ptr<BaseMsg> msg_ptr;
  };
}
