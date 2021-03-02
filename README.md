# ThreadPool
### A fast, small, efficient threadpool in c
This threadpool implementation is much faster, and much feature rich than other notable threadpool implementations found in GitHub. This threadpool has a very small footprint, and can be extensible upto hundreds of threads. 
##### Features :
1. Small initialization cost. When the function `mt_create_pool` returns, it is also guaranteed that all threads are fully initialized and in waiting state.
2. Add hundreds of jobs, asynchronously. The function `mt_add_job` is implemented to guarantee consistency and fast response.
3. Dynamically add and/or remove threads at runtime. You can add as many threads as you want using `mt_add_thread` function, or remove a thread using `mt_remove_thread` function, AT RUNTIME. Just remember, for these functions to work properly, the worker function needs to be as atomic as possible. No threads will be removed if all threads are presently busy. Also, to feel the effect of the new thread, there must be some work to be done.
4. Easy suspend and resume. You can suspend the pool anytime using `mt_suspend`, and resume where you left off using `mt_resume`.
5. Flexible stopping. You can stop the pool in two ways : a) Stop recieving new jobs, but continue existing jobs and then exit and b) Stop recieving new jobs, and exit as immediately as possible. Remeber, for the latter function to work, the work needs to be atomic. No threads will be force stopped if it is currently executing a function.
##### Proposed additions :
1. Implement force-stop [Low priority]
2. You tell me
>##### This API is also fully leak free, tested extensively using valgrind
##### License and permissions :
It is a GPL'd project. Use it anyhow, anywhere. Just mention this original repo and me as the original author of the project.
