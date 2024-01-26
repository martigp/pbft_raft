// g++ -Wall -Werror -std=c++17 testjthread.cc -o testjthread

#include <functional>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <chrono>

using namespace std;

class NamedThread {
	public:
		/**
		 * @brief Thread object.
		 */ 
		std::thread thread;

		/**
		 * Enum for: TimerThread, IncomingListeningThread, OutgoingReqThread(many)
		 * , OutgoingListeningThread, (potentially) WaitingForAppliedThread
		*/
		enum class ThreadType {
			TIMER,
			SERVERLISTENING,
			CLIENTLISTENING,
			CLIENTINITIATED
		};

		/**
		 * @brief State of this server
		*/
		ThreadType myType;

		/**
		 * @brief Start and detach thread
		*/
		// void startAndDetach(function<NamedThread*> func) {
		// 	thread = std::thread(func, this);
		// 	thread.detach();
		// }

		/**
		 * @brief Request stop.
		 */ 
		std::atomic<bool> stop_requested = false;

		std::atomic<bool> closed = false;
}; // class NamedThread

void sleep(const int seconds) {
	std::this_thread::sleep_for(std::chrono::seconds(seconds));
}

void stoppable_sleep(NamedThread *myThread) {
	while (!myThread->stop_requested) {
		sleep(1);
		cout << "sleep!" << endl;
	}
	cout << "im done!" << endl;
	myThread->closed = true;
	return;
}


int main() {
	NamedThread thr;
	thr.thread = std::thread(stoppable_sleep, &thr);
	thr.thread.detach();
	sleep(5);
	thr.stop_requested = true;
	cout << "requested stop" << endl;
	for (int i = 0; i < 5; i++) {
		sleep(1);
		cout << "hi!" << endl;
	}
	cout << std::to_string(thr.closed) << endl;
	return 0;
}


