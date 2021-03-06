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

#include "mythreads.h"
#include <inttypes.h>
#include <stdio.h>

void longJob(void *dummy) {
	volatile unsigned long long i;
	for(i = 0; i < 1000000000ULL; ++i)
		;
}

int main() {
	uint64_t size;
	printf("\nEnter the number of threads to start from : ");
	scanf("%" SCNu64, &size);
	ThreadPool *pool = mt_create_pool(size);
	printf("\nEnter number of jobs : ");
	scanf("%" SCNu64, &size);
	uint64_t i = 0;
	for(i = 0; i < size; i++) mt_add_job(pool, &longJob, NULL);
	char choice = '1';
	while(choice >= '0' && choice < '9') {
		printf("\n[CHOICE:0] Add some jobs");
		printf("\n[CHOICE:1] Get number of pending jobs");
		printf("\n[CHOICE:2] Add another thread");
		printf("\n[CHOICE:3] Remove an existing thread");
		printf("\n[CHOICE:4] Get number of threads in the pool");
		printf("\n[CHOICE:5] Wait for the pool to complete");
		printf("\n[CHOICE:6] Suspend the pool");
		printf("\n[CHOICE:7] Resume the pool");
		printf("\n[CHOICE:8] Stop the pool");
		printf("\n[CHOICE:INPUT] ");
		scanf(" %c", &choice);
		switch(choice) {
			case '0':
				printf("\n[CHOICE:INPUT] Number of jobs : ");
				scanf("%" SCNu64, &size);
				for(i = 0; i < size; i++) mt_add_job(pool, &longJob, NULL);
				break;
			case '1':
				printf("\n[CHOICE:INFO] Pending jobs %" PRIu64,
				       mt_get_job_count(pool));
				break;
			case '2': mt_add_thread(pool, 1); break;
			case '3': mt_remove_thread(pool); break;
			case '4':
				printf("\n[CHOICE:INFO] Number of threads %" PRIu64,
				       mt_get_thread_count(pool));
				break;
			case '5': mt_join(pool); break;
			case '6': mt_suspend(pool); break;
			case '7': mt_resume(pool); break;
			case '8': mt_destroy_pool(pool); return 0;
			default: break;
		}
	}
	return 0;
}
