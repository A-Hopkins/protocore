/**
 * @file system_msgs.h
 * @brief Defines the system messages
 *
 * This file provides the implementation of the messages that are used
 * throughout the system, such as state transitions and heartbeats.
 */
#pragma once
#include "declare_msg.h"
#include <cstdint>
#include <string>

namespace msg
{
  /**
   * @struct StateMsg
   * @brief Represents a message for state transitions.
   *
   * This message type is used to request state transitions between tasks.
   * It carries a single byte of data that represents the new state.
   */
  DECLARE_MESSAGE_TYPE(StateMsg)
  {
    uint8_t state; ///< The new state that is being requested.

    std::string str() const
    {
      return " State = " + std::to_string(state);
    }
  };

  /**
   * @struct StateAckMsg
   * @brief Represents an acknowledgment message for state transitions.
   *
   * This message type is used to acknowledge state transitions between tasks.
   * It carries a single byte of data that represents the new state.
   */
  DECLARE_MESSAGE_TYPE(StateAckMsg)
  {
    uint8_t     state; ///< The new state that is being acknowledged.
    std::string str() const
    {
      return " State = " + std::to_string(state);
    }
  };

  /**
   * @struct HeartbeatMsg
   * @brief Represents a periodic heartbeat message.
   *
   * This message type is used to indicate that a task is alive and functioning.
   * It carries a unique identifier and a timestamp to track the heartbeat.
   */
  DECLARE_MESSAGE_TYPE(HeartbeatMsg)
  {
    uint32_t unique_id; ///< A unique identifier for the heartbeat message.
    uint64_t timestamp; ///< The timestamp of the heartbeat message.

    std::string str() const
    {
      return " Unique_id = " + std::to_string(unique_id) +
             " timestamp = " + std::to_string(timestamp);
    }
  };

  /**
   * @struct HeartbeatAckMsg
   * @brief Represents an acknowledgment message for a heartbeat.
   *
   * This message type is used to acknowledge a heartbeat message from another task.
   * It carries the unique identifier and timestamp of the original heartbeat message.
   * Along with a timestamp to indicate the time of the acknowledgment.
   */
  DECLARE_MESSAGE_TYPE(HeartbeatAckMsg)
  {
    uint32_t orig_unique_id; ///< The unique identifier of the original heartbeat message.
    uint64_t orig_timestamp; ///< The timestamp of the original heartbeat message.
    uint64_t ack_timestamp;  ///< The timestamp of the acknowledgment message.

    std::string str() const
    {
      return " orig Unique_id = " + std::to_string(orig_unique_id) +
             " orig timestamp = " + std::to_string(orig_timestamp) +
             " ack timestamp = " + std::to_string(ack_timestamp);
    }
  };
} // namespace msg
