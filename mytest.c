/*   MyTest : An example usage of MyThreads, a threadpool API written in C
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
	while(choice>='0' && choice<'7'){
		printf("\n[CHOICE:0] Add some jobs");
		printf("\n[CHOICE:1] Add another thread");
		printf("\n[CHOICE:2] Remove an existing thread");
		printf("\n[CHOICE:3] Wait for the pool to complete");
		printf("\n[CHOICE:4] Suspend the pool");
		printf("\n[CHOICE:5] Resume the pool");
		printf("\n[CHOICE:6] Stop the pool");
		printf("\n[CHOICE:INPUT] ");
		scanf(" %c", &choice);
		switch(choice){
			case '0': printf("\n[CHOICE:INPUT] Number of jobs : ");
				  scanf("%d", &size);
				  for(i=0;i<size;i++)
					  addJobToPool(pool, &longJob, NULL);
				  break;
					
			case '1': addThreadsToPool(pool, 1);
				  break;
			case '2': removeThreadFromPool(pool);
				  break;
			case '3': waitToComplete(pool);
				  break;
			case '6': destroyPool(pool);
				  return 0;
			case '4': suspendPool(pool);
				  break;
			case '5': resumePool(pool);
				  break;
			default: break;
		}
	}
	return 0;
}
