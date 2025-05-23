/**
 * @file state_manager.cpp
 * @brief This file includes the implementation of the StateManager class.
 *
 */
#include "state_manager.h"
#include "broker.h"
#include "heart_beat.h"
#include "logger.h"
#include "msg/msg.h"
#include <iostream>

void StateManager::register_task(const std::shared_ptr<task::Task>& task)
{
  std::lock_guard<std::mutex> lock(state_mutex);
  task_states[task] = task::TaskState::NOT_STARTED;

  // Notify observer
  if (task_registration_observer)
  {
    if (auto heart_beat = dynamic_cast<HeartBeatTask*>(task_registration_observer.get()))
    {
      heart_beat->notify_task_registered(task);
    }
  }
}

void StateManager::request_state_transition(task::TaskState new_state)
{
  transition_to_state(new_state);
}

bool StateManager::demand_state_transition(task::TaskState           new_state,
                                           std::chrono::milliseconds timeout)
{
  Logger::instance().log(LogLevel::INFO, this->get_name(),
                         "demanding transition to " + task_state_to_string(new_state));
  {
    std::lock_guard<std::mutex> lock(state_mutex);
    target_state = new_state;
  }
  transition_to_state(new_state);

  std::unique_lock<std::mutex> lock(state_mutex);
  bool                         success = state_transition_cv.wait_for(
                              lock, timeout, [this, new_state] { return all_tasks_in_state(new_state); });

  if (!success)
  {
    Logger::instance().log(LogLevel::ERROR, this->get_name(),
                           "Timeout waiting for tasks to state transition to " +
                               task_state_to_string(new_state));

    for (const auto& pair : task_states)
    {
      if (pair.second != new_state)
      {
        Logger::instance().log(LogLevel::ERROR, this->get_name(),
                               "\tTask " + pair.first->get_name() + " still in state " +
                                   task_state_to_string(pair.second));
      }
    }
  }

  return success;
}

void StateManager::initialize()
{
  Logger::instance().log(LogLevel::INFO, this->get_name(), "initializing tasks...");
  start();

  for (const auto& pair : task_states)
  {
    pair.first->start();
  }
  if (!demand_state_transition(task::TaskState::IDLE))
  {
    Logger::instance().log(
        LogLevel::ERROR, this->get_name(),
        "Timeout while waiting for tasks to transition to IDLE during initialization");
  }
}

void StateManager::shutdown()
{
  Logger::instance().log(LogLevel::INFO, this->get_name(), "shutting down tasks...");

  if (!demand_state_transition(task::TaskState::STOPPED))
  {
    Logger::instance().log(LogLevel::ERROR, this->get_name(),
                           "Timeout while waiting for tasks to stop during shutdown");
  }

  for (const auto& pair : task_states)
  {
    pair.first->stop();
  }

  stop();
}

void StateManager::transition_to_state(task::TaskState new_state)
{
  Logger::instance().log(LogLevel::INFO, this->get_name(),
                         "transitioning to " + task_state_to_string(new_state));

  current_state = new_state;
  safe_publish(msg::Msg(this, msg::StateMsg{static_cast<uint8_t>(new_state)}));
}

void StateManager::process_message(const msg::Msg& msg)
{
  if (msg.get_type() == msg::Type::StateAckMsg)
  {
    handle_acknowledgment(msg);
  }
  else
  {
    Logger::instance().log(LogLevel::ERROR, this->get_name(),
                           "Unhandled message type: " + msg::msg_type_to_string(msg.get_type()));
  }
}

void StateManager::handle_acknowledgment(const msg::Msg& msg)
{
  // Get the raw pointer of the sender from the message.
  task::Task* sender_raw = msg.get_sender();
  if (!sender_raw)
  {
    Logger::instance().log(LogLevel::ERROR, this->get_name(), "State ACK from unknown sender");
    return;
  }

  // Look up the shared pointer corresponding to sender_raw.
  std::shared_ptr<task::Task> sender;
  for (const auto& pair : task_states)
  {
    if (pair.first.get() == sender_raw)
    {
      sender = pair.first;
      break;
    }
  }

  if (!sender)
  {
    Logger::instance().log(LogLevel::ERROR, this->get_name(), "State ACK from unknown sender");
    return;
  }

  // Extract the acknowledgment message data.
  const auto* ack = msg.get_data_as<msg::StateAckMsg>();
  if (!ack)
  {
    Logger::instance().log(LogLevel::ERROR, this->get_name(), "Received StateAckMsg with no data");
    return;
  }

  task::TaskState acknowledged_state = static_cast<task::TaskState>(ack->state);
  task_states[sender]                = acknowledged_state;

  Logger::instance().log(LogLevel::INFO, this->get_name(),
                         "Task " + sender->get_name() + " acknowledged transition to state " +
                             task_state_to_string(acknowledged_state));

  bool all_in_target_state = true;
  for (const auto& pair : task_states)
  {
    if (pair.second != target_state)
    {
      all_in_target_state = false;
      break;
    }
  }
  if (all_in_target_state)
  {
    state_transition_cv.notify_one();
  }
}

bool StateManager::all_tasks_in_state(task::TaskState state) const
{
  for (const auto& pair : task_states)
  {
    if (pair.second != state)
    {
      return false;
    }
  }
  return true;
}

void StateManager::mark_task_as_unresponsive(std::shared_ptr<task::Task> task)
{
  Logger::instance().log(LogLevel::ERROR, this->get_name(),
                         "Task " + task->get_name() + " marked as unresponsive, setting to " +
                             task_state_to_string(task::TaskState::ERROR));
  // Send state transition directly to the unresponsive task only
  msg::StateMsg state_msg{static_cast<uint8_t>(task::TaskState::ERROR)};
  msg::Msg      msg(this, state_msg);
  Broker::deliver_message(task, msg);

  // Update our internal state tracking (optional if you want to maintain state consistency)
  std::lock_guard<std::mutex> lock(state_mutex);
  task_states[task] = task::TaskState::ERROR;
}
