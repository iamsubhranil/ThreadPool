#ifndef MYTHREADS_H
#define MYTHREADS_H

/* The main pool structure
 * 
 * To find member descriptions, see mythreads.c .
 */
typedef struct ThreadPool ThreadPool;

/* The status enum to indicate any failure.
 * 
 * These values can be compared to all the functions
 * that returns an integer, to findout the status of
 * the execution of the function.
 */
typedef enum Status{
	MEMORY_UNAVAILABLE,
	QUEUE_LOCK_FAILED,
	QUEUE_UNLOCK_FAILED,
	SIGNALLING_FAILED,
	BROADCASTING_FAILED,
	COND_WAIT_FAILED,
	COMPLETED	
} Status;

/* Creates a new thread pool with argument number of threads. 
 * 
 * When this method returns, and if the return value is not 
 * NULL, it is assured that all threads are initialized and 
 * in waiting state. If any thread fails to initialize, 
 * typically if the pthread_create method fails, a warning 
 * message is print on the stdout. This method also can fail
 * in case of insufficient memory, which is rare, and a NULL
 * is returned in that case.
 */
ThreadPool * createPool(unsigned int);

/* Waits till all the threads in the pool are finished.
 *
 * When this method returns, it is assured that all threads
 * in the pool have finished executing, and in waiting state.
 */
void waitToComplete(ThreadPool *);

/* Destroys the argument pool.
 *
 * This method tries to stop all threads in the pool
 * immediately, and destroys any resource that the pool has
 * used in its lifetime. However, this method will not
 * return until all threads have finished processing their
 * present work. That is, this method will not halt any
 * actively executing thread. Rather, it'll wait for the
 * present jobs to complete, and will keep the threads from
 * running any new jobs. This method then joins all the
 * threads, destroys all synchronization objects, and frees
 * any remaining jobs, finally freeing the pool itself.
 */
void destroyPool(ThreadPool *);

/* Add a new job to the pool.
 *
 * This method adds a new job, that is a worker function,
 * to the pool. The execution of the function is performed
 * asynchronously, however. This method only assures the
 * addition of the job to the job queue. The job queue is
 * ordered in FIFO style, i.e., for this job to execute,
 * all the jobs that has been added previously has to be
 * executed first. This method doesn't guarantee the thread
 * on which the job may execute. Rather, when its turn comes,
 * the thread which first becomes idle, executes this job.
 * When all threads are idle, any one of them wakes up and
 * executes this function asynchronously.
 */
int addJobToPool(ThreadPool *, void (*func)(void *), void *);

/* Add some new threads to the pool.
 * 
 * This function adds specified number of new threads to the 
 * argument threadpool. When this function returns, it is 
 * ensured that a new thread has been added to the pool. 
 * However, this new thread will only come to effect if there 
 * are remainder jobs, that is the job queue is not presently 
 * empty. This new thread will not steal any running jobs 
 * from the running threads. Occasionally, this method will 
 * return some error codes, typically due to the failure of 
 * pthread_create, or for insufficient memory. These error 
 * codes can be compared using the Status enum above.
 */
int addThreadsToPool(ThreadPool *, int);

/* Remove an existing thread from the pool.
 *
 * This function will remove one thread from the threadpool,
 * asynchronously. That is, this method will not stop any
 * active threads, rather it'll merely indicate the wish.
 * When any active thread will become idle, before becoming
 * active again the thread will check if removal is wished.
 * If it is wished, then thread will immediately exit. This
 * method can run N times to remove N threads, however it
 * has some serious consequences. If N is greater than the
 * number of threads present in the pool, say M, then all
 * M threads will be stopped. However, next (N-M) threads
 * will also immediately exit when added to the pool. If
 * all M threads are removed from the queue, then the job
 * queue will halt, and when a new thread will be added to
 * the pool, the queue will automatically resume from the
 * position where it stopped.
 */
void removeThreadFromPool(ThreadPool *);

#endif
