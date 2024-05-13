#ifndef RAFT_SHELLSTATEMACHINE_H
#define RAFT_SHELLSTATEMACHINE_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "Protobuf/RaftRPC.pb.h"
#include "RaftServer/RaftServer.hh"

namespace Raft {

class RaftServer;

class ShellStateMachine {
 public:
  /**
   * @brief Construct a new ShellStateMachine that applies log entries
   * received from the server. Enforces the property of one command executing
   * at a time, using a queue and a callback per command.
   *
   * Any variation of StateMachine can be
   * created that matches the public interface of this ShellStateMachine.
   *
   * @param callbackFn Method provided to the shell state machine to
   * be invoked when a new entry has been applied. Allows the state machine
   * communicate the result of applying the entry at a specific index.
   * CallbackFn requires two arguments: uint64_t index
   *                                    std::string result
   */
  ShellStateMachine(std::function<void(uint64_t, std::string*)> callbackFn);

  /* Destructor */
  ~ShellStateMachine();

  /**
   * @brief Method used by RaftServer to indicate to the State Machine that
   * commitIndex has been updated, along with new entries.
   *
   * RaftServer is responsible for pushing commands once and in order.
   */
  void pushCmd(uint64_t index, std::string cmd);

 private:
  /**
   * @brief State Machine Loop
   * Flow of events:
   * Received new entry on the queue
   * Apply the entry
   * Use the callback function to return a result
   *      NOTE: so that there is only one writer to a file, RaftServer will
   *      write to persistent state to update lastApplied after StateMahcine
   * calls back to it. There are two possibilities for point of failure given
   * that linearizability is not within the scope of this project: lastApplied
   * written to disk and command fails to complete before crash OR command
   * completes and lastApplied fails to be written to disk before crash. Our
   * implementation uses the latter.
   */
  void stateMachineLoop();

  /**
   * @brief Executes the provided command locally.
   *
   * @param command Command to executed
   * @return std::string The shell response from running the command
   */
  std::string* applyCmd(const std::string& cmd);

  /**
   * @brief Function provided by RaftServer that will accept as arguments a
   * log index and a result once they have been applied
   */
  std::function<void(uint64_t, std::string*)> callbackRaftServer;

  /**
   * @brief StateMachine Updates CV
   * Triggered by RaftServer committing new log entries
   * New highest commitIndex has been stored
   */
  std::condition_variable stateMachineUpdatesCV;

  /**
   * @brief Mutex to synchronize access to the commandQueue
   */
  std::mutex commandQueueMutex;

  /**
   * @brief Queue of outstanding indices and entries to commit.
   */
  struct StateMachineCommand {
    uint64_t index;
    std::string command;
  };

  std::queue<StateMachineCommand> commandQueue;

};  // class ShellStateMachine
}  // namespace Raft

#endif /* RAFT_SHELLSTATEMACHINE_H */