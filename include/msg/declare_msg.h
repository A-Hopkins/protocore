/**
 * @file declare_msg.h
 * @brief This file provides a macro for declaring message types.
 * 
 * This macro should be used for declaring all message types.
 */

#pragma once

// Standardize the declaration of a message type without forcing per-type header files.
// This macro can be used inside a grouped header.
#define DECLARE_MESSAGE_TYPE(TYPE) struct TYPE
