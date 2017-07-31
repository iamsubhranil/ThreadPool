#ifndef MYTHREADS_H
#define MYTHREADS_H

typedef struct ThreadPool ThreadPool;

ThreadPool * createPool(unsigned int);
void waitToComplete(ThreadPool *);
void destroyPool(ThreadPool *);
int addJobToPool(ThreadPool *, void (*func)(void *), void *);
int addThreadsToPool(ThreadPool *, int);
void removeThreadFromPool(ThreadPool *);
#endif
