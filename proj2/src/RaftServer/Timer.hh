#ifndef RAFT_TIMER_H
#define RAFT_TIMER_H

#include <string>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>
#include "RaftServer/RaftServer.hh"

namespace Raft {

    class RaftServer;

    class Timer {
        public:
            /**
             * Create a new Timer object for the Raft Consensus Algorithm
             * 
             * @param server The Raft Server that responds to timer events.
            */
            Timer(RaftServer* server);

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

    }; // class Timer
} // namespace Raft

#endif /* RAFT_TIMER_H */