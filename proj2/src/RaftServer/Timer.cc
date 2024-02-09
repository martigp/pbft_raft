#include "Timer.hh"

namespace Raft {

    Timer::Timer(std::function<void()> callbackFn) 
        : timerTimeout(10000)
        , timerReset(false)
    {
        callbackFn = std::bind(callbackFn); // TODO: fix this bind thing to have a reference to RaftServer
        std::thread t = std::thread(&Timer::timerLoop, this);
        t.detach();
    }

    // TODO: How do I destruct?
    Timer::~Timer() {}

    void Timer::resetTimer(std::optional<uint64_t> newTimeout = std::nullopt) {
        std::unique_lock<std::mutex> lock(resetTimerMutex);
        if (newTimeout) {
            timerTimeout = newTimeout.value();
        }
        timerReset = true;
        timerResetCV.notify_all();
    }

    void Timer::timerLoop() {
        printf("[Timer.cc]: Starting timer with timeout of %llu ms.\n", timerTimeout);
        while (true) {
            std::unique_lock<std::mutex> lock(resetTimerMutex);
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