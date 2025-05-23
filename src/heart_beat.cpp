/**
 * @file heart_beat.cpp
 * @brief This file includes the implementation of the HeartBeat class.
 *
 */
#include "heart_beat.h"
#include "broker.h"
#include "state_manager.h"
#include <iostream>

void HeartBeatTask::notify_task_registered(std::shared_ptr<task::Task> task)
{
  if (task.get() == this)
  {
    return;
  }

  std::lock_guard<std::mutex> lock(tasks_mutex);
  if (monitored_tasks.find(task) == monitored_tasks.end())
  {
    TaskStatus status;
    status.last_response_time = std::chrono::steady_clock::now();
    monitored_tasks[task]     = status;
  }
}

void HeartBeatTask::on_initialize()
{
  safe_subscribe(msg::Type::StateMsg);
  safe_subscribe(msg::Type::HeartbeatAckMsg);

  // Register with StateManager as an observer
  if (state_manager)
  {
    state_manager->set_task_registration_observer(shared_from_this());
  }
}

void HeartBeatTask::process_message(const msg::Msg& msg)
{
  switch (msg.get_type())
  {
    case msg::Type::StateMsg:
    {
      auto stateMsg = msg.get_data_as<msg::StateMsg>();
      transition_to_state(static_cast<task::TaskState>(stateMsg->state));
      break;
    }

    case msg::Type::HeartbeatAckMsg:
    {
      handle_acknowledgment(msg);
      break;
    }

    default:
      std::cout << "Unhandled message type: " << msg::msg_type_to_string(msg.get_type())
                << std::endl;
      break;
  }
}

void HeartBeatTask::periodic_task_process()
{
  check_for_timeouts();

  if (get_current_state() == task::TaskState::RUNNING)
  {
    // Send heartbeat if in RUNNING state
    send_heartbeat();
  }
}

uint32_t HeartBeatTask::generate_heartbeat_id()
{
  return next_heartbeat_id++;
}

void HeartBeatTask::check_for_timeouts()
{
  std::lock_guard<std::mutex> lock(tasks_mutex);
  auto                        current_time = std::chrono::steady_clock::now();

  for (auto& [task, status] : monitored_tasks)
  {
    if (status.awaiting_response)
    {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          current_time - status.last_heartbeat_time);

      if (elapsed > response_timeout_ms)
      {
        std::cout << "Task " << task->get_name()
                  << " failed to respond to Heartbeat ID: " << status.last_heartbeat_id
                  << " Taking action: " << task_state_to_string(unresponsive_action) << std::endl;
        handle_unresponsive_task(task);
        status.awaiting_response = false;
      }
    }
  }
}

void HeartBeatTask::send_heartbeat()
{
  std::lock_guard<std::mutex> lock(tasks_mutex);

  for (auto& [task, status] : monitored_tasks)
  {
    if (!status.awaiting_response)
    {
      auto     current_time = std::chrono::steady_clock::now();
      uint32_t heartbeat_id = generate_heartbeat_id();
      uint64_t timestamp =
          std::chrono::duration_cast<std::chrono::milliseconds>(current_time.time_since_epoch())
              .count();
      msg::HeartbeatMsg heartbeat{heartbeat_id, timestamp};
      msg::Msg          heartbeat_msg(this, heartbeat);
      status.awaiting_response   = true;
      status.last_heartbeat_id   = heartbeat_id;
      status.last_heartbeat_time = current_time;

      // Note: Using direct delivery instead of safe_publish because:
      // 1. Heartbeats are targeted to specific tasks
      // 2. We need to track which specific task gets each heartbeat ID
      // 3. The monitoring relationship is 1:1 between HeartBeatTask and each monitored task
      Broker::deliver_message(task, heartbeat_msg);
    }
  }
}

void HeartBeatTask::handle_acknowledgment(const msg::Msg& msg)
{
  auto                        ack        = msg.get_data_as<msg::HeartbeatAckMsg>();
  task::Task*                 sender_raw = msg.get_sender();
  std::lock_guard<std::mutex> lock(tasks_mutex);

  for (auto& [task, status] : monitored_tasks)
  {
    if (task.get() == sender_raw)
    {
      if (status.last_heartbeat_id == ack->orig_unique_id)
      {
        status.awaiting_response  = false;
        status.last_response_time = std::chrono::steady_clock::now();
        std::cout << "Received Heartbeat ACK from " << task->get_name()
                  << " for Heartbeat ID: " << ack->orig_unique_id
                  << " at time: " << ack->ack_timestamp << std::endl;
      }
      break;
    }
  }
}

void HeartBeatTask::handle_unresponsive_task(std::shared_ptr<task::Task> task)
{
  if (state_manager)
  {
    state_manager->mark_task_as_unresponsive(task);
  }
}
