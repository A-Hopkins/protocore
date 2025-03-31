/**
 * @file msg_varient_types.h
 * @brief Defines the message variant types and associated priorities.
 * 
 * The list below is used to create:
 *   - A variant type (MessageVariant) that can hold any of the listed message types.
 *   - A constexpr array (MessagePriorities) that holds the priority for each message type.
 *
 * To add a new message type, add a new line (with the type name and its priority)
 * and then declare the message using the DECLARE_MESSAGE_TYPE macro.
 */
#pragma once
#include <variant>

#include "system_msgs.h"

namespace msg
{
  // List all message types and their priorities here.
  #ifndef MESSAGE_VARIANT_TYPES
  #define MESSAGE_VARIANT_TYPES(X)      \
      X(StateMsg,                100),  \
      X(StateAckMsg,              99),  \
      X(HeartbeatMsg,              50), \
      X(HeartbeatAckMsg,           49)
  #endif
  // Message type declarations are provided in "system_msgs.h"
  // Define the MessageVariant type using the list of message types.
  #define MSG_VARIANT_TYPE(TYPE, PRIORITY) TYPE
  using MessageVariant = std::variant<MESSAGE_VARIANT_TYPES(MSG_VARIANT_TYPE)>;
  // Define the Type enum using the list of message types.
  enum class Type
  {
    MESSAGE_VARIANT_TYPES(MSG_VARIANT_TYPE)
  };
  #undef MSG_VARIANT_TYPE

  // Then, generate a compile-time array of priorities corresponding to each variant alternative:
  #define MSG_VARIANT_TYPE(TYPE, PRIORITY) PRIORITY
  using Priority = uint16_t;
  constexpr Priority message_priorities[] = { MESSAGE_VARIANT_TYPES(MSG_VARIANT_TYPE) };
  #undef MSG_VARIANT_TYPE
}
