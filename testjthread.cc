// g++ -Wall -Werror -std=c++17 testjthread.cc -o testjthread

#include <functional>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <chrono>
#include <memory>
#include <map>
#include <vector>

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

void globals_test(NamedThread &thr) {
	// std::unique_ptr<NamedThread> ret = std::make_unique<NamedThread>();
	// ret->thread = std::make_shared<std::thread>(stoppable_sleep, &ret);

	// NamedThread thr;
	thr.thread = std::thread(stoppable_sleep, &thr);

	// thr.thread.detach();
	sleep(5);
	thr.stop_requested = true;
	cout << "requested stop" << endl;
	for (int i = 0; i < 5; i++) {
		sleep(1);
		cout << "hi!" << endl;
	}
	cout << std::to_string(thr.closed) << endl;
	if (thr.closed) {
		cout << "joining in globals!" << endl;
		thr.thread.join();
	}
	// auto ret = std::make_unique<NamedThread>(thr);
	return;
}

class ThreadHolder {
	public:
		ThreadHolder(int n) : timerThread()
		{
			persistentThreads = std::vector<NamedThread>(n);
		}

		NamedThread timerThread;

		std::vector<NamedThread> persistentThreads;
};

int main() {
	// NamedThread thr;
	int num;
    cout << "Enter the integer: ";
    cin >> num;

	ThreadHolder mine(num);
	globals_test(mine.persistentThreads[0]); 
	NamedThread *thr = &mine.persistentThreads[0];
	for (int i = 0; i < 5; i++) {
		sleep(1);
		cout << "back in main!" << endl;
	}
	cout << std::to_string(thr->closed) << endl;
	if (!thr->closed) {
		cout << "joining in main and requesting stop!" << endl;
		thr->stop_requested = true;
		thr->thread.join();
	}
	cout << "reuse object!" << endl;
	thr->stop_requested = false;
	thr->closed = false;
	thr->thread = std::thread(stoppable_sleep, thr);
	sleep(3);
	cout << "joining in main and requesting stop!" << endl;
	thr->stop_requested = true;
	thr->thread.join();

	cout << "start something on end thread number: " << to_string(num) << endl;
	thr = &mine.persistentThreads[num - 1];
	thr->thread = std::thread(stoppable_sleep, thr);
	cout << "started sleep" << endl;
	sleep(3);
	cout << "requesting stop and joining" << endl;
	thr->stop_requested = true;
	thr->thread.join();
	cout << "done!" << endl;

	return 0;
}

// int main() {
// 	NamedThread thr;
// 	thr.thread = std::thread(stoppable_sleep, &thr);
// 	// thr.thread.detach();
// 	sleep(5);
// 	thr.stop_requested = true;
// 	cout << "requested stop" << endl;
// 	for (int i = 0; i < 5; i++) {
// 		sleep(1);
// 		cout << "hi!" << endl;
// 	}
// 	cout << std::to_string(thr.closed) << endl;
// 	if (thr.closed) {
// 		thr.thread.join();
// 	}
// 	return 0;
// }

/**
 * Terminal output:
 * Enter the integer: 6
sleep!
sleep!
sleep!
sleep!
sleep!
hi!
sleep!
hi!
sleep!
hi!
sleep!
hi!
sleep!
hi!
0
sleep!
back in main!
sleep!
back in main!
sleep!
back in main!
sleep!
back in main!
sleep!
back in main!
0
joining in main and requesting stop!
sleep!
im done!
reuse object!
sleep!
sleep!
joining in main and requesting stop!
sleep!
im done!
start something on end thread number: 6
started sleep
sleep!
sleep!
requesting stop and joining
sleep!
im done!
done!
*/

