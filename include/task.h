

#pragma once

#include "msg/msg.h"
#include "message_queue.h"

namespace task
{

  class Task
  {
  public:
    Task() : message_queue(512, 64) { }

    MessageQueue message_queue; ///< The message queue for storing incoming messages.
  };
}