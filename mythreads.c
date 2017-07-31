#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include"mythreads.h"
#define DEBUG

typedef enum Status{
	MEMORY_UNAVAILABLE,
	QUEUE_LOCK_FAILED,
	QUEUE_UNLOCK_FAILED,
	SIGNALLING_FAILED,
	BROADCASTING_FAILED,
	COND_WAIT_FAILED,
	COMPLETED	
} Status;

typedef struct ThreadList {
	pthread_t thread;
	struct ThreadList *next;
} ThreadList;

typedef struct Job {
	void (*function)(void *);
	void *args;
	struct Job *next;
} Job;

struct ThreadPool {
	ThreadList * threads;
	ThreadList * rearThreads;
	unsigned int numThreads;
	unsigned int waitingThreads;
	unsigned short isInitialized;
	pthread_mutex_t queuemutex;
	pthread_mutex_t condmutex;
	pthread_cond_t conditional;
	_Atomic unsigned short run;
	unsigned int threadID;
	Job *FRONT;
	Job *REAR;
	pthread_mutex_t endmutex;
	pthread_cond_t endconditional;
};

static void printQueue(Job *head){
	Job *temp = head;
	int i = 0;
	while(temp){
		temp = temp->next;
		i++;
	}
	printf("\n[THREADPOOL:STAT:INFO] Remaining jobs : %d", i);
}

static void *threadExecutor(void *pl){
	ThreadPool *pool = (ThreadPool *)pl;
	int rc = pthread_mutex_lock(&pool->queuemutex);
	if(rc){
		printf("\n[THREADPOOL:THREAD:WARNING] Unable to lock the mutex (error code %d)! Will result in wrong thread id!", rc);
	}
	unsigned int id = ++pool->threadID;
	rc = pthread_mutex_unlock(&pool->queuemutex);
	if(rc){
		printf("\n[THREADPOOL:THREAD%u:WARNING] Unable to unlock the mutex (error code %d)!", id, rc);
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:THREAD%u:INFO] Starting execution loop!", id);
#endif

	while(pool->run){
#ifdef DEBUG
		printf("\n[THREADPOOL:THREAD%u:INFO] Trying to lock the mutex!", id);
#endif

		rc = pthread_mutex_lock(&pool->queuemutex);
		if(rc){
			printf("\n[THREADPOOL:THREAD%u:ERROR] Unable to lock the mutex (error code %d)!", id, rc);
			pthread_exit((void *)QUEUE_LOCK_FAILED);
		}
		Job *presentJob = pool->FRONT;
		if(presentJob==NULL){

#ifdef DEBUG
			printf("\n[THREADPOOL:THREAD%u:INFO] Queue is empty! Unlocking the mutex!", id);
#endif
			pool->waitingThreads++;
#ifdef DEBUG
			printf("\n[THREADPOOL:THREAD%u:INFO] Waiting threads %u!", id, pool->waitingThreads);
#endif
			if(pool->waitingThreads==pool->numThreads){
#ifdef DEBUG
				printf("\n[THREADPOOL:THREAD%u:INFO] All threads are idle now!", id);
#endif
				if(pool->isInitialized){
#ifdef DEBUG
					printf("\n[THREADPOOL:THREAD%u:INFO] Signaling endconditional!" ,id);
					fflush(stdout);
#endif
					pthread_mutex_lock(&pool->endmutex);
					pthread_cond_signal(&pool->endconditional);
					pthread_mutex_unlock(&pool->endmutex);
#ifdef DEBUG
					printf("\n[THREADPOOL:THREAD%u:INFO] Signalled any monitor!", id);
#endif
				}
				else
					pool->isInitialized = 1;
			}

			rc = pthread_mutex_unlock(&pool->queuemutex);
			if(rc){
				printf("\n[THREADPOOL:THREAD%u:ERROR] Unable to unlock the mutex (error code %d)!", id, rc);
				pthread_exit((void *)QUEUE_UNLOCK_FAILED);
			}

#ifdef DEBUG
			printf("\n[THREADPOOL:THREAD%u:INFO] Going to conditional wait!\n", id);
#endif
			pthread_mutex_lock(&pool->condmutex);
			rc = pthread_cond_wait(&pool->conditional, &pool->condmutex);
			pthread_mutex_unlock(&pool->condmutex);
			if(rc){
				printf("\n[THREADPOOL:THREAD%u:ERROR] Conditional wait failed (error code %d)!", id, rc);
				pthread_exit((void *)COND_WAIT_FAILED);
			}

#ifdef DEBUG
			printf("\n[THREADPOOL:THREAD%u:INFO] Woke up from conditional wait!", id);
#endif			
		}
		else{
			pool->FRONT = pool->FRONT->next;
			if(pool->FRONT==NULL)
				pool->REAR = NULL;
#ifdef DEBUG
			else
				printQueue(pool->FRONT);
#endif
			if(pool->waitingThreads>0)
				pool->waitingThreads--;
#ifdef DEBUG
			printf("\n[THREADPOOL:THREAD%u:INFO] Job recieved! Unlocking the mutex!", id);
#endif
			rc = pthread_mutex_unlock(&pool->queuemutex);
			if(rc){
				printf("\n[THREADPOOL:THREAD%u:ERROR] Unable to unlock the mutex (error code %d)!", id, rc);
				pthread_exit((void *)QUEUE_UNLOCK_FAILED);
			}

#ifdef DEBUG
			printf("\n[THREADPOOL:THREAD%u:INFO] Executing the job now!", id);
#endif

			presentJob->function(presentJob->args);

#ifdef DEBUG
			printf("\n[THREADPOOL:THREAD%u:INFO] Job completed! Releasing memory for the job!", id);
#endif

			free(presentJob);
		}
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:THREAD%u:INFO] Pool has been stopped! Exiting now..", id);
#endif

	pthread_exit((void *)COMPLETED);
}

int addThreadsToPool(ThreadPool *pool, int threads){
	pthread_mutex_lock(&pool->queuemutex);
	int rc = 0;
	pool->numThreads += threads;
	int i = 0;
	for(i=0;i<threads;i++){

		ThreadList *newThread = (ThreadList *)malloc(sizeof(ThreadList));
		newThread->next = NULL;
		rc = pthread_create(&newThread->thread, NULL, threadExecutor, (void *)pool);
		if(rc){
			printf("\n[THREADPOOL:INIT:ERROR] Unable to create thread %d(error code %d)!", (i+1), rc);
			pool->numThreads--;
		}
		else{
#ifdef DEBUG
			printf("\n[THREADPOOL:INIT:INFO] Initialized thread %u!", (i+1));
#endif
			if(pool->rearThreads==NULL)
				pool->threads = pool->rearThreads = newThread;
			else
				pool->rearThreads->next = newThread;
			pool->rearThreads = newThread;
		}
	}
	pthread_mutex_unlock(&pool->queuemutex);
	return rc;
}

ThreadPool * createPool(unsigned int numThreads){
	ThreadPool * pool = (ThreadPool *)malloc(sizeof(ThreadPool));
	if(pool==NULL){
		printf("[THREADPOOL:INIT:ERROR] Unable to allocate memory for the pool!");
		return NULL;
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:INIT:INFO] Allocated %lu bytes for new pool!", sizeof(ThreadPool));
#endif

	pool->numThreads = 0;
	pool->FRONT = NULL;
	pool->REAR = NULL;
	pool->waitingThreads = 0;
	pool->isInitialized = 0;
	int mrc = pthread_mutex_init(&pool->queuemutex, NULL);
	if(mrc){
		printf("\n[THREADPOOL:INIT:ERROR] Unable to initialize mutex(error code %u)!", mrc);
		free(pool);
		return NULL;
	}
	mrc = pthread_cond_init(&pool->conditional, NULL);
	if(mrc){
		printf("\n[THREADPOOL:INIT:ERROR] Unable to initialize conditional(error code %u)!", mrc);
	}
	mrc = pthread_mutex_init(&pool->condmutex, NULL);
	mrc = pthread_mutex_init(&pool->endmutex, NULL);
	mrc = pthread_cond_init(&pool->endconditional, NULL);

	pool->run = 1;

#ifdef DEBUG
	printf("\n[THREADPOOL:INIT:INFO] Successfully initialized all members of the pool!");
	printf("\n[THREADPOOL:INIT:INFO] Initializing %u threads..",numThreads);
#endif
	addThreadsToPool(pool, numThreads);

#ifdef DEBUG
	printf("\n[THREADPOOL:INIT:INFO] Waiting for all threads to start..");
#endif

	while(pool->waitingThreads<numThreads);

#ifdef DEBUG
	printf("\n[THREADPOOL:INIT:INFO] New threadpool initialized successfully!");
#endif

	return pool;
}

int addJobToPool(ThreadPool *pool, void (*func)(void *args), void *args){
	Job *newJob = (Job *)malloc(sizeof(Job));
	if(newJob==NULL){
		printf("\n[THREADPOOL:EXEC:ERROR] Unable to allocate memory for new job!");
		return MEMORY_UNAVAILABLE;
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:EXEC:INFO] Allocated %lu bytes for new job!", sizeof(Job));
#endif

	newJob->function = func;
	newJob->args = args;
	newJob->next = NULL;

#ifdef DEBUG
	printf("\n[THREADPOOL:EXEC:INFO] Locking the queue for insertion of the job!");
#endif

	int rc = pthread_mutex_lock(&pool->queuemutex);
	if(rc){
		printf("\n[THREADPOOL:EXEC:ERROR] Unable to lock the queue!");
		free(newJob);
		return QUEUE_LOCK_FAILED;
	}
	if(pool->FRONT==NULL)
		pool->FRONT = pool->REAR = newJob;
	else
		pool->REAR->next = newJob;
	pool->REAR = newJob;

#ifdef DEBUG
	printf("\n[THREADPOOL:EXEC:INFO] Inserted the job at the end of the queue!");
#endif

	if(pool->waitingThreads>0){
#ifdef DEBUG
		printf("\n[THREADPOOL:EXEC:INFO] Signaling any idle thread!");
#endif
		pthread_mutex_lock(&pool->condmutex);
		rc = pthread_cond_signal(&pool->conditional);
		pthread_mutex_unlock(&pool->condmutex);
		if(rc){
			printf("\n[THREADPOOL:EXEC:WARNING] Unable to signal any idle threads!");
		}

#ifdef DEBUG
		printf("\n[THREADPOOL:EXEC:INFO] Signaling successful!");
#endif
	}
	rc = pthread_mutex_unlock(&pool->queuemutex);
	if(rc){
		printf("\n[THREADPOOL:EXEC:ERROR] Unable to unlock the queue(error code %d)! Pool should be destroyed now!", rc);
		return QUEUE_UNLOCK_FAILED;
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:EXEC:INFO] Unlocked the mutex!");
#endif
	return 0;
}

void waitToComplete(ThreadPool *pool){
	pthread_mutex_lock(&pool->endmutex);
	pthread_cond_wait(&pool->endconditional, &pool->endmutex);
	pthread_mutex_unlock(&pool->endmutex);
}

void destroyPool(ThreadPool *pool){
#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Trying to wakeup all waiting threads..");
#endif
	pool->run = 0;
	int rc = pthread_cond_broadcast(&pool->conditional);
	if(rc){
		printf("\n[THREADPOOL:EXIT:WARNING] Broadcasting failed! One or more threads may still be active!");
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Waiting for all threads to exit..");
#endif

	ThreadList *list = pool->threads, *backup = NULL;

	Status stat;
	void *c = &stat;
	unsigned int i = 0;
	while(list!=NULL){

#ifdef DEBUG
		printf("\n[THREADPOOL:EXIT:INFO] Joining thread %u..", i);
#endif

		rc = pthread_join(list->thread, &c);
		if(rc)
			printf("\n[THREADPOOL:EXIT:WARNING] Unable to join THREAD%u!", i);

#ifdef DEBUG		
		else
			printf("\n[THREADPOOL:EXIT:INFO] THREAD%u joined!", i);
#endif

		backup = list;
		list = list->next;

#ifdef DEBUG
		printf("\n[THREADPOOL:EXIT:INFO] Releasing memory for THREAD%u..", i);
#endif

		free(backup);
		i++;
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Destroying remaining jobs..");
#endif

	while(pool->FRONT!=NULL){
		Job *j = pool->FRONT;
		pool->FRONT = pool->FRONT->next;
		free(j);
	}

#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Destroying conditional..");
#endif
	rc = pthread_cond_destroy(&pool->conditional);
	if(rc)
		printf("\n[THREADPOOL:EXIT:WARNING] Unable to destroy the conditional (error code %d)!", rc);

#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Destroying the mutex..");
#endif

	rc = pthread_mutex_destroy(&pool->queuemutex);
	if(rc)
		printf("\n[THREADPOOL:EXIT:WARNING] Unable to destroy the conditional (error code %d)!", rc);

	pthread_mutex_destroy(&pool->condmutex);
	pthread_mutex_destroy(&pool->endmutex);
	pthread_cond_destroy(&pool->endconditional);
#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Releasing memory for the pool..");
#endif

	free(pool);
#ifdef DEBUG
	printf("\n[THREADPOOL:EXIT:INFO] Pool destruction completed!");
#endif
}
