mythreads: mythreads.c mythreads.h mytest.c
	clang -O3 mytest.c mythreads.c -lpthread -o mytest
mythreads_debug: mythreads.c mythreads.h mytest.c
	clang -g mytest.c mythreads.c -lpthread -o mytest_debug
