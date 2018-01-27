all: mythreads.c mythreads.h mytest.c
	clang -O3 mytest.c mythreads.c -lpthread -o mytest

debug: mythreads.c mythreads.h mytest.c
	clang -g -Wall -DDEBUG mytest.c mythreads.c -lpthread -o mytest_debug

clean:
	rm mytest
	rm mytest_debug
