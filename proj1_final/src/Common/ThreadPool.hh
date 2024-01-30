#ifndef RAFT_THREADPOOL_H
#define RAFT_THREADPOOL_H

#include <cstdlib>
#include <functional>
#include <thread>
#include <semaphore>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>

/**
 * @brief The minimum maximum value of a counting semaphore. Arbitrarily
 * set to 1, but max value is determined by the constructor.
 * 
 */
#define MIN_COUNTER_VALUE


namespace Raft {

class ThreadPool {
    public:

        /**
         * @brief Constructs a ThreadPool configured to spawn up to the 
         * specified number of worker threads.
         */
        ThreadPool(size_t numThreads);

        /**
         * @brief Destructor
         */
        ~ThreadPool();

        /**
         * Schedules the provided job (which is something that can
         * be invoked as a zero-argument function without a return value)
         * to be executed by one of the ThreadPool's threads as soon as
         * all previously scheduled jobs have been handled.
         */
        void schedule(const std::function<void(void *args)>& fn,
                      void *args);

        /**
         * Blocks and waits until all previously scheduled jobs
         * have been executed in full.
         */
        void wait();

        struct WorkerJob {
            std::function<void(void *args)> fn;
            void *args;
        };

    private:

        /**
         * @brief Job consisting of a function and a set of arguments.
         * The function must be of the type:
         * void fn(void *args) and is responsible for parsing arguments
         * from the generic pointer.
         */

        /**
         * @brief Control information for a dispatcher thread to coordinate
         * worker threads.
         */
        class Worker {
            public:
                /**
                 * @brief Constructs a new worker with its counting semaphore
                 * set to 0 so that it waits for work to be assigned.
                 */
                Worker();

                /**
                 * @brief Semaphore used by dispatch thread to signal the thread
                 * has work to do. Once signaled, the thread exexutes its job.
                 */
                std::counting_semaphore<1> workToBeDone;

                /**
                 * @brief The thread that will execute the worker's job.
                 */
                std::thread thread;

                /**
                 * @brief The job to be done by the worker, consisting of
                 * a function and its arguments. This is removed from the
                 * work Queue in the threadpool and called by the worker.
                 */
                struct WorkerJob job;
                

                /**
                 * @brief Flag used to indicate that the worker is available
                 * to a job. Dispatcher checks this value and sets to false
                 * when a job is assigned. Worker sets to true when job is
                 * is complete.
                 */
                bool free;  
        };

        /**
         * @brief Thread responsible for assigning jobs to worker threads.
         */
        std::thread dispatchThread;

        /**
         * @brief Threads performing jobs assigned by the dispatcher thread.
         */
        std::vector<Worker> workers;

        /**
         * @brief A synchronized counter to measure the number of workers that
         * are available. Signaled when worker completes its job. Decremented
         * when worker assigned work.
         */
        std::counting_semaphore<1> availableWorkers;

        /**
         * @brief Synchronization to allow dispatch thread to start running
         * once all workers have been initialized.
         */
        std::counting_semaphore<1> scheduleDispatch;

        /**
         * @brief Synchronization for the queue of jobs to be done.
         */
        std::mutex jobQueueLock;

        /**
         * @brief Queue of jobs to be done by the threadpool
         */
        std::queue<WorkerJob> jobs;

        /**
         * @brief Lock for accessing the condition variable associated with
         * the number of free threads.
         */
        std::mutex numThreadsFreeLock;

        size_t numThreadsFree;

        /**
         * @brief Condition Variable used to check whether any threads are
         * free to do work.
         */
        std::condition_variable_any numThreadsFreeCv;

        /**
         * @brief Flag set to shut down the threadpool.
         */
        bool shutdown;
  

        /**
         * @brief The function executed by the dispatch thread to give work
         * to worker threads.
         */
        
        void dispatcher();

        /**
         * @brief Function executed by a worker thread that waits for and then
         * executes a job. Note this is different from the job it has been
         * given.
         */
        
        void worker(size_t workerID);

        ThreadPool(const ThreadPool& original) = delete;
        ThreadPool& operator=(const ThreadPool& rhs) = delete;
};

} // namespace RAFT_THREADPOOL_H
#endif
