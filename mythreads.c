/*   MyThreads : A small, efficient, and fast threadpool implementation in C
 *   Copyright (C) 2017  Subhranil Mukherjee (https://github.com/iamsubhranil)
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, version 3 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mythreads.h" // API header
#include <inttypes.h>  // Standard integer format specifiers
#include <pthread.h>   // The thread library
#include <stdio.h>     // Standard output functions in case of errors and debug
#include <stdlib.h>    // Memory management functions

/* A singly linked list of threads. This list
 * gives tremendous flexibility managing the
 * threads at runtime.
 */
typedef struct ThreadList {
	pthread_t          thread; // The thread object
	struct ThreadList *next;   // Link to next thread
} ThreadList;

/* A singly linked list of worker functions. This
 * list is implemented as a queue to manage the
 * execution in the pool.
 */
typedef struct Job {
	void (*function)(void *); // The worker function
	void *      args;         // Argument to the function
	struct Job *next;         // Link to next Job
} Job;

/* The core pool structure. This is the only
 * user accessible structure in the API. It contains
 * all the primitives necessary to provide
 * synchronization between the threads, along with
 * dynamic management and execution control.
 */
struct ThreadPool {
	/* The FRONT of the thread queue in the pool.
	 * It typically points to the first thread
	 * created in the pool.
	 */
	ThreadList *threads;

	/* The REAR of the thread queue in the pool.
	 * Points to the last, and most young thread
	 * added to the pool.
	 */
	ThreadList *rearThreads;

	/* Number of threads in the pool. As this can
	 * grow dynamically, access and modification
	 * of it is bounded by a mutex.
	 */
	uint64_t numThreads;

	/* The indicator which indicates the number
	 * of threads to remove. If this is equal to
	 * N, then N threads will be removed from the
	 * pool when they are idle. All threads
	 * typically check the value of this variable
	 * before executing a job, and if finds the
	 * value >0, immediately exits.
	 */
	uint64_t removeThreads;

	/* Denotes the number of idle threads in the
	 * pool at any given instant of time. This value
	 * is used to check if all threads are idle,
	 * and thus triggering the end of job queue or
	 * the initialization of the pool, whichever
	 * applicable.
	 */
	volatile uint64_t waitingThreads;

	/* Denotes whether the pool is presently
	 * initalized or not. This variable is used to
	 * busy wait after the creation of the pool
	 * to ensure all threads are in waiting state.
	 */
	volatile uint8_t isInitialized;

	/* The main mutex for the job queue. All
	 * operations on the queue is done after locking
	 * this mutex to ensure consistency.
	 */
	pthread_mutex_t queuemutex;

	/* This mutex indicates whether a thread is
	 * presently in idle state or not, and is used
	 * in conjunction with the conditional below.
	 */
	pthread_mutex_t condmutex;

	/* Conditional to ensure conditional wait.
	 * When idle, each thread waits on this
	 * conditional, which is signaled by various
	 * methods to indicate the wake of the thread.
	 */
	pthread_cond_t conditional;

	/* Ensures pool state. When the pool is running,
	 * this is set to 1. All the threads loop on
	 * this condition, and exits immediately when
	 * it is set to 0, which happens when the pool
	 * is destroyed.
	 */
	_Atomic uint8_t run;

	/* Used to assign unique thread IDs to each
	 * running threads. It is an always incremental
	 * counter.
	 */
	uint64_t threadID;

	/* The FRONT of the job queue, which typically
	 * points to the job to be executed next.
	 */
	Job *FRONT;

	/* The REAR of the job queue, which points
	 * to the job last added in the pool.
	 */
	Job *REAR;

	/* Mutex used to denote the end of the job
	 * queue, which triggers the function
	 * waitForComplete.
	 */
	pthread_mutex_t endmutex;

	/* Conditional to signal the end of the job
	 * queue.
	 */
	pthread_cond_t endconditional;

	/* Variable to impose and withdraw
	 * the suspend state.
	 */
	uint8_t suspend;

	/* Counter to the number of jobs
	 * present in the job queue
	 */
	_Atomic uint64_t jobCount;
};

/* The core function which is executed in each thread.
 * A pointer to the pool is passed as the argument,
 * which controls the flow of execution of the thread.
 */
static void *threadExecutor(void *pl) {
	ThreadPool *pool = (ThreadPool *)pl;   // Get the pool
	pthread_mutex_lock(&pool->queuemutex); // Lock the mutex
	++pool->threadID;                      // Get an id

#ifdef DEBUG
	uint64_t id = pool->threadID;
#endif

	pthread_mutex_unlock(&pool->queuemutex); // Release the mutex

#ifdef DEBUG
	printf("\n[THREADPOOL:THREAD%" PRIu64 ":INFO] Starting execution loop!",
	       id);
#endif
	// Start the core execution loop
	while(pool->run) { // run==1, we should get going
#ifdef DEBUG
		printf("\n[THREADPOOL:THREAD%" PRIu64
		       ":INFO] Trying to lock the mutex!",
		       id);
#endif

		pthread_mutex_lock(&pool->queuemutex); // Lock the queue mutex

		if(pool->removeThreads > 0) { // A thread is needed to be removed
#ifdef DEBUG
			printf("\n[THREADPOOL:THREAD%" PRIu64
			       ":INFO] Removal signalled! Exiting the execution loop!",
			       id);
#endif
			pthread_mutex_lock(&pool->condmutex);
			pool->numThreads--;
			pthread_mutex_unlock(&pool->condmutex);
			break; // Exit the loop
		}
		Job *presentJob = pool->FRONT;            // Get the first job
		if(presentJob == NULL || pool->suspend) { // Queue is empty!

#ifdef DEBUG
			if(presentJob == NULL)
				printf("\n[THREADPOOL:THREAD%" PRIu64
				       ":INFO] Queue is empty! Unlocking the mutex!",
				       id);
			else
				printf("\n[THREADPOOL:THREAD%" PRIu64
				       ":INFO] Suspending thread!",
				       id);
#endif
			pthread_mutex_unlock(&pool->queuemutex); // Unlock the mutex

			pthread_mutex_lock(&pool->condmutex); // Hold the conditional mutex
			pool->waitingThreads++; // Add yourself as a waiting thread
#ifdef DEBUG
			printf("\n[THREADPOOL:THREAD%" PRIu64
			       ":INFO] Waiting threads %" PRIu64 "!",
			       id, pool->waitingThreads);
#endif
			if(!pool->suspend && pool->waitingThreads ==
			                         pool->numThreads) { // All threads are idle
#ifdef DEBUG
				printf("\n[THREADPOOL:THREAD%" PRIu64
				       ":INFO] All threads are idle now!",
				       id);
#endif
				if(pool->isInitialized) { // Pool is initialized, time to
					                      // trigger the end conditional
#ifdef DEBUG
					printf("\n[THREADPOOL:THREAD%" PRIu64
					       ":INFO] Signaling endconditional!",
					       id);
					fflush(stdout);
#endif
					pthread_mutex_lock(&pool->endmutex); // Lock the mutex
					pthread_cond_signal(
					    &pool->endconditional);            // Signal the end
					pthread_mutex_unlock(&pool->endmutex); // Release the mutex
#ifdef DEBUG
					printf("\n[THREADPOOL:THREAD%" PRIu64
					       ":INFO] Signalled any monitor!",
					       id);
#endif
				} else                       // We are initializing the pool
					pool->isInitialized = 1; // Break the busy wait
			}

#ifdef DEBUG
			printf("\n[THREADPOOL:THREAD%" PRIu64
			       ":INFO] Going to conditional wait!",
			       id);
			fflush(stdout);
#endif
			pthread_cond_wait(&pool->conditional,
			                  &pool->condmutex); // Idle wait on conditional

			/* Woke up! */

			if(pool->waitingThreads >
			   0) // Unregister youself as a waiting thread
				pool->waitingThreads--;

			pthread_mutex_unlock(
			    &pool->condmutex); // Woke up! Release the mutex

#ifdef DEBUG
			printf("\n[THREADPOOL:THREAD%" PRIu64
			       ":INFO] Woke up from conditional wait!",
			       id);
#endif
		} else { // There is a job in the pool

			pool->FRONT = pool->FRONT->next; // Shift FRONT to right
			pool->jobCount--;                // Decrement the count

			if(pool->FRONT == NULL) // No jobs next
				pool->REAR = NULL;  // Reset the REAR
#ifdef DEBUG
			else
				printf("\n[THREADPOOL:THREAD%" PRIu64
				       ":INFO] Remaining jobs : %" PRIu64,
				       id, pool->jobCount);

			printf("\n[THREADPOOL:THREAD%" PRIu64
			       ":INFO] Job recieved! Unlocking the mutex!",
			       id);
#endif

			pthread_mutex_unlock(&pool->queuemutex); // Unlock the mutex

#ifdef DEBUG
			printf("\n[THREADPOOL:THREAD%" PRIu64
			       ":INFO] Executing the job now!",
			       id);
			fflush(stdout);
#endif

			presentJob->function(presentJob->args); // Execute the job

#ifdef DEBUG
			printf("\n[THREADPOOL:THREAD%" PRIu64
			       ":INFO] Job completed! Releasing memory for the job!",
			       id);
#endif

			free(presentJob); // Release memory for the job
		}
	}

	if(pool->run) { // We exited, but the pool is running! It must be force
		            // removal!
#ifdef DEBUG
		printf("\n[THREADPOOL:THREAD%" PRIu64 ":INFO] Releasing the lock!", id);
#endif
		pool->removeThreads--; // Alright, I'm shutting now
		pthread_mutex_unlock(
		    &pool->queuemutex); // We broke the loop, release the mutex now
#ifdef DEBUG
		printf("\n[THREADPOOL:THREAD%" PRIu64 ":INFO] Stopping now..", id);
		fflush(stdout);
#endif
	}
#ifdef DEBUG
	else // The pool is stopped
		printf("\n[THREADPOOL:THREAD%" PRIu64
		       ":INFO] Pool has been stopped! Exiting now..",
		       id);
#endif

	pthread_exit((void *)COMPLETED); // Exit
}

/* This method adds 'threads' number of new threads
 * to the argument pool. See header for more details.
 */
ThreadPoolStatus mt_add_thread(ThreadPool *pool, uint64_t threads) {
	if(pool == NULL) { // Sanity check
		printf("\n[THREADPOOL:ADD:ERROR] Pool is not initialized!");
		return POOL_NOT_INITIALIZED;
	}
	if(!pool->run) {
		printf("\n[THREADPOOL:ADD:ERROR] Pool already stopped!");
		return POOL_STOPPED;
	}
	if(threads < 1) {
		printf(
		    "\n[THREADPOOL:ADD:WARNING] Tried to add invalid number of threads "
		    "%" PRIu64 "!",
		    threads);
		return INVALID_NUMBER;
	}

	int              temp = 0;
	ThreadPoolStatus rc   = COMPLETED;
#ifdef DEBUG
	printf("\n[THREADPOOL:ADD:INFO] Holding the condmutex..");
#endif
	pthread_mutex_lock(&pool->condmutex);
	pool->numThreads +=
	    threads; // Increment the thread count to prevent idle signal
	pthread_mutex_unlock(&pool->condmutex);
#ifdef DEBUG
	printf("\n[THREADPOOL:ADD:INFO] Speculative increment done!");
#endif
	uint64_t i = 0;
	for(i = 0; i < threads; i++) {

		ThreadList *newThread =
		    (ThreadList *)malloc(sizeof(ThreadList)); // Allocate a new thread
		newThread->next = NULL;
		temp = pthread_create(&newThread->thread, NULL, threadExecutor,
		                      (void *)pool); // Start the thread
		if(temp) {
			printf("\n[THREADPOOL:ADD:ERROR] Unable to create thread %" PRIu64
			       "(error code %d)!",
			       (i + 1), temp);
			pthread_mutex_lock(&pool->condmutex);
			pool->numThreads--;
			pthread_mutex_unlock(&pool->condmutex);
			temp = 0;
			rc   = THREAD_CREATION_FAILED;
		} else {
#ifdef DEBUG
			printf("\n[THREADPOOL:ADD:INFO] Initialized thread %" PRIu64 "!",
			       (i + 1));
#endif
			if(pool->rearThreads == NULL) // This is the first thread
				pool->threads = pool->rearThreads = newThread;
			else // There are threads in the pool
				pool->rearThreads->next = newThread;
			pool->rearThreads = newThread; // This is definitely the last thread
		}
	}
	return rc;
}

/* This method removes one thread from the
 * argument pool. See header for more details.
 */
void mt_remove_thread(ThreadPool *pool) {
	if(pool == NULL || !pool->isInitialized) {
		printf("\n[THREADPOOL:REM:ERROR] Pool is not initialized!");
		return;
	}
	if(!pool->run) {
		printf(
		    "\n[THREADPOOL:REM:WARNING] Removing thread from a stopped pool!");
		return;
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:REM:INFO] Acquiring the lock!");
#endif
	pthread_mutex_lock(&pool->queuemutex); // Lock the mutex
#ifdef DEBUG
	printf("\n[THREADPOOL:REM:INFO] Incrementing the removal count");
#endif
	pool->removeThreads++; // Indicate the willingness of removal
	pthread_mutex_unlock(&pool->queuemutex); // Unlock the mutex
#ifdef DEBUG
	printf("\n[THREADPOOL:REM:INFO] Waking up any sleeping threads!");
#endif
	pthread_mutex_lock(&pool->condmutex);    // Lock the wait mutex
	pthread_cond_signal(&pool->conditional); // Signal any idle threads
	pthread_mutex_unlock(&pool->condmutex);  // Release the wait mutex
#ifdef DEBUG
	printf("\n[THREADPOOL:REM:INFO] Signalling complete!");
#endif
}

/* This method creates a new thread pool containing
 * argument number of threads. See header for more
 * details.
 */

ThreadPool *mt_create_pool(uint64_t numThreads) {
	ThreadPool *pool = (ThreadPool *)malloc(
	    sizeof(ThreadPool)); // Allocate memory for the pool
	if(pool == NULL) {       // Oops!
		printf(
		    "[THREADPOOL:INIT:ERROR] Unable to allocate memory for the pool!");
		return NULL;
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:INIT:INFO] Allocated %zu bytes for new pool!",
	       sizeof(ThreadPool));
#endif
	// Initialize members with default values
	pool->numThreads     = 0;
	pool->FRONT          = NULL;
	pool->REAR           = NULL;
	pool->waitingThreads = 0;
	pool->isInitialized  = 0;
	pool->removeThreads  = 0;
	pool->suspend        = 0;
	pool->rearThreads    = NULL;
	pool->threads        = NULL;
	pool->jobCount       = 0;
	pool->threadID       = 0;

#ifdef DEBUG
	printf("\n[THREADPOOL:INIT:INFO] Initializing mutexes!");
#endif

	pthread_mutex_init(&pool->queuemutex, NULL); // Initialize queue mutex
	pthread_mutex_init(&pool->condmutex, NULL);  // Initialize idle mutex
	pthread_mutex_init(&pool->endmutex, NULL);   // Initialize end mutex

#ifdef DEBUG
	printf("\n[THREADPOOL:INIT:INFO] Initiliazing conditionals!");
#endif

	pthread_cond_init(&pool->endconditional,
	                  NULL);                     // Initialize end conditional
	pthread_cond_init(&pool->conditional, NULL); // Initialize idle conditional

	pool->run = 1; // Start the pool

#ifdef DEBUG
	printf(
	    "\n[THREADPOOL:INIT:INFO] Successfully initialized all members of the "
	    "pool!");
	printf("\n[THREADPOOL:INIT:INFO] Initializing %" PRIu64 " threads..",
	       numThreads);
#endif

	if(numThreads < 1) {
		printf("\n[THREADPOOL:INIT:WARNING] Starting with no threads!");
		pool->isInitialized = 1;
	} else {
		mt_add_thread(pool, numThreads); // Add threads to the pool
#ifdef DEBUG
		printf("\n[THREADPOOL:INIT:INFO] Waiting for all threads to start..");
#endif
	}

	while(!pool->isInitialized)
		; // Busy wait till the pool is initialized

#ifdef DEBUG
	printf("\n[THREADPOOL:INIT:INFO] New threadpool initialized successfully!");
#endif

	return pool;
}

/* Adds a new job to pool. See header for more
 * details.
 *
 */
ThreadPoolStatus mt_add_job(ThreadPool *pool, void (*func)(void *args),
                              void *      args) {
	if(pool == NULL || !pool->isInitialized) { // Sanity check
		printf("\n[THREADPOOL:EXEC:ERROR] Pool is not initialized!");
		return POOL_NOT_INITIALIZED;
	}
	if(!pool->run) {
		printf(
		    "\n[THREADPOOL:EXEC:ERROR] Trying to add a job in a stopped pool!");
		return POOL_STOPPED;
	}
	if(pool->run == 2) {
		printf("\n[THREADPOOL:EXEC:WARNING] Another thread is waiting for the "
		       "pool "
		       "to complete!");
		return WAIT_ISSUED;
	}

	Job *newJob = (Job *)malloc(sizeof(Job)); // Allocate memory
	if(newJob == NULL) {                      // Who uses 2KB RAM nowadays?
		printf(
		    "\n[THREADPOOL:EXEC:ERROR] Unable to allocate memory for new job!");
		return MEMORY_UNAVAILABLE;
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:EXEC:INFO] Allocated %zu bytes for new job!",
	       sizeof(Job));
#endif

	newJob->function = func; // Initialize the function
	newJob->args     = args; // Initialize the argument
	newJob->next     = NULL; // Reset the link

#ifdef DEBUG
	printf(
	    "\n[THREADPOOL:EXEC:INFO] Locking the queue for insertion of the job!");
#endif

	pthread_mutex_lock(&pool->queuemutex); // Inserting the job, lock the queue

	if(pool->FRONT == NULL) // This is the first job
		pool->FRONT = pool->REAR = newJob;
	else // There are other jobs
		pool->REAR->next = newJob;
	pool->REAR = newJob; // This is the last job

	pool->jobCount++; // Increment the count

#ifdef DEBUG
	printf(
	    "\n[THREADPOOL:EXEC:INFO] Inserted the job at the end of the queue!");
#endif

	if(pool->waitingThreads >
	   0) { // There are some threads sleeping, wake'em up
#ifdef DEBUG
		printf("\n[THREADPOOL:EXEC:INFO] Signaling any idle thread!");
#endif
		pthread_mutex_lock(&pool->condmutex);    // Lock the mutex
		pthread_cond_signal(&pool->conditional); // Signal the conditional
		pthread_mutex_unlock(&pool->condmutex);  // Release the mutex

#ifdef DEBUG
		printf("\n[THREADPOOL:EXEC:INFO] Signaling successful!");
#endif
	}

	pthread_mutex_unlock(&pool->queuemutex); // Finally, release the queue

#ifdef DEBUG
	printf("\n[THREADPOOL:EXEC:INFO] Unlocked the mutex!");
#endif
	return COMPLETED;
}

/* Wait for the pool to finish executing. See header
 * for more details.
 */
void mt_join(ThreadPool *pool) {
	if(pool == NULL || !pool->isInitialized) { // Sanity check
		printf("\n[THREADPOOL:WAIT:ERROR] Pool is not initialized!");
		return;
	}
	if(!pool->run) {
		printf("\n[THREADPOOL:WAIT:ERROR] Pool already stopped!");
		return;
	}

	pool->run = 2;

	pthread_mutex_lock(&pool->condmutex);
	if(pool->numThreads == pool->waitingThreads) {
#ifdef DEBUG
		printf("\n[THREADPOOL:WAIT:INFO] All threads are already idle!");
#endif
		pthread_mutex_unlock(&pool->condmutex);
		pool->run = 1;
		return;
	}
	pthread_mutex_unlock(&pool->condmutex);
#ifdef DEBUG
	printf("\n[THREADPOOL:WAIT:INFO] Waiting for all threads to become idle..");
#endif
	pthread_mutex_lock(&pool->endmutex); // Lock the mutex
	pthread_cond_wait(&pool->endconditional,
	                  &pool->endmutex);    // Wait for end signal
	pthread_mutex_unlock(&pool->endmutex); // Unlock the mutex
#ifdef DEBUG
	printf("\n[THREADPOOL:WAIT:INFO] All threads are idle now!");
#endif
	pool->run = 1;
}

/* Suspend all active threads in a pool. See header
 * for more details.
 */
void mt_suspend(ThreadPool *pool) {
	if(pool == NULL || !pool->isInitialized) { // Sanity check
		printf("\n[THREADPOOL:SUSP:ERROR] Pool is not initialized!");
		return;
	}
	if(!pool->run) { // Pool is stopped
		printf("\n[THREADPOOL:SUSP:ERROR] Pool already stopped!");
		return;
	}
	if(pool->suspend) { // Pool is already suspended
		printf("\n[THREADPOOL:SUSP:ERROR] Pool already suspended!");
		return;
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:SUSP:INFO] Initiating suspend..");
#endif
	pthread_mutex_lock(&pool->queuemutex);   // Lock the queue
	pool->suspend = 1;                       // Present the wish for suspension
	pthread_mutex_unlock(&pool->queuemutex); // Release the queue
#ifdef DEBUG
	printf("\n[THREADPOOL:SUSP:INFO] Waiting for all threads to be idle..");
	fflush(stdout);
#endif
	while(pool->waitingThreads < pool->numThreads)
		; // Busy wait till all threads are idle
#ifdef DEBUG
	printf("\n[THREADPOOL:SUSP:INFO] Successfully suspended all threads!");
#endif
}

/* Resume a suspended pool. See header for more
 * details.
 */
void mt_resume(ThreadPool *pool) {
	if(pool == NULL || !pool->isInitialized) { // Sanity check
		printf("\n[THREADPOOL:RESM:ERROR] Pool is not initialized!");
		return;
	}
	if(!pool->run) { // Pool stopped
		printf("\n[THREADPOOL:RESM:ERROR] Pool is not running!");
		return;
	}
	if(!pool->suspend) { // Pool is not suspended
		printf("\n[THREADPOOL:RESM:WARNING] Pool is not suspended!");
		return;
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:RESM:INFO] Initiating resume..");
#endif
	pthread_mutex_lock(&pool->condmutex); // Lock the conditional
	pool->suspend = 0;                    // Reset the state
#ifdef DEBUG
	printf("\n[THREADPOOL:RESM:INFO] Waking up all threads..");
#endif
	pthread_cond_broadcast(&pool->conditional); // Wake up all threads
	pthread_mutex_unlock(&pool->condmutex);     // Release the mutex
#ifdef DEBUG
	printf("\n[THREADPOOL:RESM:INFO] Resume complete!");
#endif
}

/* Returns number of pending jobs in the pool. See
 * header for more details
 */
uint64_t mt_get_job_count(ThreadPool *pool) {
	return pool->jobCount;
}

/* Returns the number of threads in the pool. See
 * header for more details.
 */
uint64_t mt_get_thread_count(ThreadPool *pool) {
	return pool->numThreads;
}

/* Destroy the pool. See header for more details.
 *
 */
void mt_destroy_pool(ThreadPool *pool) {
	if(pool == NULL || !pool->isInitialized) { // Sanity check
		printf("\n[THREADPOOL:EXIT:ERROR] Pool is not initialized!");
		return;
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Trying to wakeup all waiting threads..");
#endif
	pool->run = 0; // Stop the pool

	pthread_mutex_lock(&pool->condmutex);
	pthread_cond_broadcast(&pool->conditional); // Wake up all idle threads
	pthread_mutex_unlock(&pool->condmutex);

	int rc;
#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Waiting for all threads to exit..");
#endif

	ThreadList *list = pool->threads, *backup = NULL; // For travsersal

	uint64_t i = 0;
	while(list != NULL) {

#ifdef DEBUG
		printf("\n[THREADPOOL:EXIT:INFO] Joining thread %" PRIu64 "..", i);
#endif

		rc = pthread_join(list->thread, NULL); //  Wait for ith thread to join
		if(rc)
			printf("\n[THREADPOOL:EXIT:WARNING] Unable to join THREAD%" PRIu64
			       "!",
			       i);

#ifdef DEBUG
		else
			printf("\n[THREADPOOL:EXIT:INFO] THREAD%" PRIu64 " joined!", i);
#endif

		backup = list;
		list   = list->next; // Continue

#ifdef DEBUG
		printf("\n[THREADPOOL:EXIT:INFO] Releasing memory for THREAD%" PRIu64
		       "..",
		       i);
#endif

		free(backup); // Free ith thread
		i++;
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Destroying remaining jobs..");
#endif

	// Delete remaining jobs
	while(pool->FRONT != NULL) {
		Job *j      = pool->FRONT;
		pool->FRONT = pool->FRONT->next;
		free(j);
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Destroying conditionals..");
#endif
	rc =
	    pthread_cond_destroy(&pool->conditional); // Destroying idle conditional
	rc = pthread_cond_destroy(
	    &pool->endconditional); // Destroying end conditional
	if(rc)
		printf("\n[THREADPOOL:EXIT:WARNING] Unable to destroy one or more "
		       "conditionals (error code %d)!",
		       rc);

#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Destroying the mutexes..");
#endif

	rc = pthread_mutex_destroy(&pool->queuemutex); // Destroying queue lock
	rc = pthread_mutex_destroy(&pool->condmutex);  // Destroying idle lock
	rc = pthread_mutex_destroy(&pool->endmutex);   // Destroying end lock
	if(rc)
		printf("\n[THREADPOOL:EXIT:WARNING] Unable to destroy one or mutexes "
		       "(error code %d)!",
		       rc);

#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Releasing memory for the pool..");
#endif

	free(pool); // Release the pool
#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Pool destruction completed!");
#endif
}
