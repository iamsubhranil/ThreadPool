#include<stdio.h>
#include"mythreads.h"

void longJob(void *dummy){
	volatile unsigned long long i;
 	for (i = 0; i < 1000000000ULL; ++i);
}

int main(){
	int size;
	printf("\nEnter the number of threads to start from : ");
	scanf("%d", &size);
	ThreadPool *pool = createPool(size);
	printf("\nEnter number of jobs : ");
	scanf("%d", &size);
	int i = 0;
	for(i=0;i<size;i++)
		addJobToPool(pool, &longJob, NULL);
	char choice = '1';
	while(choice>'0' && choice<'5'){
		printf("\n[CHOICE:1] Add another thread");
		printf("\n[CHOICE:2] Remove an existing thread");
		printf("\n[CHOICE:3] Wait for the pool to complete");
		printf("\n[CHOICE:4] Stop the pool");
		printf("\n[CHOICE:INPUT] ");
		scanf(" %c", &choice);
		switch(choice){
			case '1': addThreadsToPool(pool, 1);
				  break;
			case '2': removeThreadFromPool(pool);
				  break;
			case '3': waitToComplete(pool);
				  break;
			case '4': destroyPool(pool);
				  break;
			default: break;
		}
	}
	return 0;
}
