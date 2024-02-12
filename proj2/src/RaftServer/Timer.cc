#include "Timer.hh"

namespace Raft {

    Timer::Timer(std::function<void()> callbackFn) 
        : callbackFn( callbackFn )
        , timerTimeout(10000)
        , timerReset(false)
    {
        std::thread t = std::thread(&Timer::timerLoop, this);
        t.detach();
    }

    // TODO: How do I destruct?
    Timer::~Timer() {}

    void Timer::resetTimer(const std::optional<uint64_t>& newTimeout) {
        std::unique_lock<std::mutex> lock(resetTimerMutex);
        if (newTimeout.has_value()) {
            timerTimeout = newTimeout.value();
        }
        timerReset = true;
        timerResetCV.notify_all();
    }

    void Timer::timerLoop() {
        printf("[Timer.cc]: Starting timer with timeout of %llu ms.\n", timerTimeout);
        while (true) {
            std::unique_lock<std::mutex> lock(resetTimerMutex);
            // Returns true if the timer was reset before the timer timed out.
            // False if timer was not reset before timer timed out.
            if (timerResetCV.wait_for(lock, std::chrono::milliseconds(timerTimeout), [&]{ return timerReset == true; })) {
                timerReset = false;
                printf("[Timer.cc]: Timer reset, timeout of %llu ms.\n", timerTimeout);
                lock.unlock();
                continue;
            } else {
                printf("[Timer.cc]: Timed out, calling callback function.\n");
                lock.unlock();
                callbackFn();
            }
        }
    }

}