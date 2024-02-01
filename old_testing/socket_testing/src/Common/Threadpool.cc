#include "Common/ThreadPool.hh"
#include <mutex>
#include <memory>
#include <iostream>

namespace Raft {
    
    ThreadPool::Worker::Worker() 
    : workToBeDone(0),
      free(true)
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

    void ThreadPool::schedule(const std::function<void(void *args)>& fn,
                              void *args) {

        numThreadsFreeLock.lock();
        numThreadsFree++;
        numThreadsFreeLock.unlock();

        struct WorkerJob newWorkerJob;
        newWorkerJob.fn = fn;
        newWorkerJob.args = args;
        
        jobQueueLock.lock();
        jobs.push(std::move(newWorkerJob));
        jobQueueLock.unlock();

        scheduleDispatch.release();
        printf("Job scheduled\n");

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
        for(Worker& worker: workers) {
            worker.workToBeDone.release();
        }
        dispatchThread.join();
        for(Worker& worker: workers) {       
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
                for (Worker &worker: workers) {
                    if(worker.free) {
                        worker.free = false;
                        worker.job = std::move(jobs.front());
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
        try{
        while(true) {
            Worker& worker = workers[workerID];
            worker.workToBeDone.acquire();

            if (shutdown)
                break;
            
            printf("[ThreadPool] Worker %zu about to start new job\n", workerID);
            worker.job.fn(worker.job.args);

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
        catch(const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }

}



