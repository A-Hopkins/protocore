/**
 * @file system_msgs.h
 * @brief Defines the system messages
 *
 * This file provides the implementation of the messages that are used
 * throughout the system, such as state transitions and heartbeats.
 */
#pragma once

#include "base_msg.h"

namespace msg
{
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
      TypedMsg(sender, state, 950) { }
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
      TypedMsg(sender, state, 949) { }
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
      TypedMsg(sender, { unique_id, timestamp }, 900) { }

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
      TypedMsg(sender, { orig_unique_id, orig_timestamp, ack_timestamp }, 899) { }

    uint32_t get_orig_unique_id() const { return get_data()->orig_unique_id; }
    uint32_t get_orig_timestamp() const { return get_data()->orig_timestamp; }
    uint32_t get_ack_timestamp() const { return get_data()->ack_timestamp; }
  };
}
