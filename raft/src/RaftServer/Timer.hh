#ifndef RAFT_TIMER_H
#define RAFT_TIMER_H

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "RaftServer/RaftServer.hh"

namespace Raft {

class RaftServer;

class Timer {
 public:
  /**
   * Create a new Timer object with a specified callback function
   * to indicate when a timeout has occured.
   * Will be used as apart of the Raft Consensus Algorithm.
   *
   * @param callbackFn The method given to the timer to be invoked
   * on timeouts
   */
  Timer(std::function<void()> callbackFn);

  /**
   * Destructor for Timer object
   */
  ~Timer();

  /**
   * @brief Resets timer with an option for indicating a new timeout value
   *
   * @param newTimeout Optionally provided to indicate new timeout
   */
  void resetTimer(std::optional<uint64_t> newTimeout = std::nullopt);

 private:
  /**
   * @brief Main function loop that timer thread will execute
   */
  void timerLoop();

  /**
   * @brief Function provided by RaftServer to indicate timer
   * has elapsed
   */
  std::function<void()> callbackFn;

  /**
   * @brief Current timeout length in milliseconds
   * Defaults to 10000 ms on boot up
   */
  uint64_t timerTimeout;

  /**
   * @brief Timer reset CV, triggered by calls to
   * ResetTimer method
   */
  std::condition_variable timerResetCV;

  /**
   * @brief Mutex access to timerTimeout and timerReset
   */
  std::mutex resetTimerMutex;

  /**
   * @brief Avoid spurious wakeups of the timer CV
   * true if timer is to be reset, false otherwise
   */
  bool timerReset;

};  // class Timer
}  // namespace Raft

#endif /* RAFT_TIMER_H */