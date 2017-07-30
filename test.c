#include<stdio.h>
#include"mythreads.h"
#include<unistd.h>
_Atomic int val;

void dummyJob(void *dummy){
	int i = val++;
	printf("\n[DUMMYJOB%d] Sleeping for 2 seconds!\n", i);
	sleep(1);
	printf("\n[DUMMYJOB%d] Woke up!\n", i);
}

int main(){
	int sizepool, sizejobs;
	printf("\nEnter the size of the pool : ");
	scanf("%d", &sizepool);
	ThreadPool *pool = createPool(sizepool);
	printf("\nEnter the number of jobs : ");
	scanf("%d", &sizejobs);
	printf("\nPushing jobs..");
	int i = 0;
	//sleep(2);
	for(i=0;i<sizejobs;i++){
		addJobToPool(pool, &dummyJob, NULL);
	}
	printf("\nWaiting for pool to finish..");
	waitToComplete(pool);
	printf("\nDestroying pool..");
	destroyPool(pool);
	return 0;
}
