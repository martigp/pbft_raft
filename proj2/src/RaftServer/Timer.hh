#ifndef RAFT_TIMER_H
#define RAFT_TIMER_H

#include <string>
#include <functional>

namespace Raft {

    class Timer {
        public:
            /**
             * Create a new Timer object for the Raft Consensus Algorithm
            */
            Timer();

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
             * @brief Function provided by RaftServer to indicate timer
             * has elapsed
            */
            std::function<void()> callbackFn;

            /**
             * @brief Current timeout length in milliseconds
            */
            uint64_t timerTimeout;


    }; // class Timer
} // namespace Raft

#endif /* RAFT_TIMER_H */