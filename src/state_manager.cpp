#include <iostream>

#include "broker.h"
#include "msg/msg.h"
#include "state_manager.h"

StateManager::StateManager() : task::Task("StateManager")
{
  Broker::subscribe(std::shared_ptr<StateManager>(this), msg::Type::StateAckMsg);
}

void StateManager::register_task(task::Task* task)
{
  task_states[task] = task::TaskState::NOT_STARTED;
}

void StateManager::request_state_transition(task::TaskState new_state)
{
  transition_to_state(new_state);
}

bool StateManager::demand_state_transition(task::TaskState new_state, std::chrono::seconds timeout)
{
  std::cout << name << " demanding transition to " << task_state_to_string(new_state) << std::endl;
  {
    std::lock_guard<std::mutex> lock(state_mutex);
    target_state = new_state;
  }
  transition_to_state(new_state);

  std::unique_lock<std::mutex> lock(state_mutex);
  bool success = state_transition_cv.wait_for(lock, timeout, [this, new_state] {
    return all_tasks_in_state(new_state);
  });

  if (!success)
  {
    std::cerr << "Timeout waiting for tasks to state transition to " << task_state_to_string(new_state) << std::endl;

    for (const auto& pair : task_states)
    {
      if (pair.second != new_state)
      {
        std::cerr << "\tTask " << pair.first->get_name() << " still in state " << task_state_to_string(pair.second) << std::endl;
      }
    }
  }

  return success;
}

void StateManager::initialize()
{
  std::cout << name << " initializing tasks...\n";
  start();

  for (const auto& pair : task_states)
  {
    pair.first->start();
  }
  if (!demand_state_transition(task::TaskState::IDLE))
  {
    std::cerr << "Error: Timeout while waiting for tasks to transition to IDLE during initialization\n";
  }
}

void StateManager::shutdown()
{
  std::cout << name << " shutting down tasks...\n";

  if (!demand_state_transition(task::TaskState::STOPPED))
  {
    std::cerr << "Error: Timeout while waiting for tasks to stop during shutdown\n";
  }

  for (const auto& pair : task_states)
  {
    pair.first->stop();
  }

  stop();
}


void StateManager::transition_to_state(task::TaskState new_state)
{
  std::cout << name << " transitioning to " << task_state_to_string(new_state) << "\n";

  current_state = new_state;
  Broker::publish(msg::Msg(this, msg::StateMsg{static_cast<uint8_t>(new_state)}));
                           
}

void StateManager::process_message(const msg::Msg& msg)
{
  if (msg.get_type() == msg::Type::StateAckMsg)
  {
    handle_acknowledgment(msg);
  }
  else
  {
    std::cout << "Unhandled message type: " << msg::msg_type_to_string(msg.get_type()) << std::endl;
  }
}

void StateManager::handle_acknowledgment(const msg::Msg& msg)
{
  auto sender_task = msg.get_sender();

  if (task_states.find(sender_task) == task_states.end())
  {
    std::cerr << "Error: State ACK from unknown sender\n";
    return;
  }
  const auto* ack = msg.get_data_as<msg::StateAckMsg>();

  if (!sender_task)
  {
    std::cerr << "Error: State ACK from unknown sender\n";
    return;
  }

  if (!ack)
  {
    std::cerr << "Error: Received StateAckMsg with no data\n";
    return;
  }

  task::TaskState acknowledged_state = static_cast<task::TaskState>(ack->state);
  task_states[sender_task] = acknowledged_state;
  std::cout << "Task " << sender_task->get_name() << " acknowledged transition to state " << task_state_to_string(acknowledged_state) << "\n";

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
