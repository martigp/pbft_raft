#include "Common/Threadpool.hh"
#include <mutex>

namespace Raft {
    
    ThreadPool::WorkerInfo::WorkerInfo() 
    : workToBeDone(1)
    {
    }

    ThreadPool::ThreadPool(size_t numThreads) 
        : workers(numThreads),
          availableWorkers(numThreads),
          scheduleDispatch(0),
          jobQueueLock(),
          jobs(),
          numThreadsFreeLock(),
          numThreadsFree(0),
          shutdown(false) 
    {
        
        dispatchThread = std::thread([this]() {
                dispatcher();
        });

        for (size_t workerID = 0; workerID < numThreads; workerID++) {
            workers[workerID].thread = std::thread([this](size_t workerID) {
                    worker(workerID);
                }, workerID);
            workers[workerID].free = true;
        }
    }

    void ThreadPool::schedule(const std::function<void(void)>& thunk) {

        numThreadsFreeLock.lock();
        numThreadsFree++;
        numThreadsFreeLock.unlock();

        jobQueueLock.lock();
        jobs.push(thunk);
        jobQueueLock.unlock();

        scheduleDispatch.release();
    }

    void ThreadPool::wait() {
        std::lock_guard<std::mutex> lg(numThreadsFreeLock);
        numThreadsFreeCv.wait(numThreadsFreeLock,
                              [this]{return numThreadsFree == 0;});
    }

    ThreadPool::~ThreadPool() {
        wait();
        shutdown = true;
        scheduleDispatch.release();
        availableWorkers.release();
        for(WorkerInfo& worker: workers) {
            worker.workToBeDone.release();
        }
        dispatchThread.join();
        for(WorkerInfo& worker: workers) {       
            worker.thread.join();
        }
    }

    void ThreadPool::dispatcher(){
        while(true) {
            scheduleDispatch.acquire();
            availableWorkers.acquire();
            if (shutdown) break;
            jobQueueLock.lock();
            if (!jobs.empty()) {
                //jobQueueLock.unlock();
            //} else {
                for (WorkerInfo &worker: workers) {
                    if(worker.free) {
                        worker.free = false;
                        worker.job = jobs.front();
                        jobs.pop();
                        worker.workToBeDone.release();
                        //jobQueueLock.unlock();//check this is in the correct order
                        break;
                    }
                }  
            }
            jobQueueLock.unlock();
        }
    }

    void ThreadPool::worker(size_t workerID) {
        while(true) {
            WorkerInfo& worker = workers[workerID];
            worker.workToBeDone.acquire();

            if (shutdown)
                break;
            
            worker.job();

            worker.free = true;
            availableWorkers.release();
            
            // Signal to dispatcher that finished with job
            numThreadsFreeLock.lock();
            numThreadsFree--;
            if (numThreadsFree == 0) {
                numThreadsFreeCv.notify_all();
            }
            numThreadsFreeLock.unlock();
        }
    }

}



