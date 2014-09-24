
# simple tester makefile

all: runtest

webtpl_test:	webtpl_test.c ../webtpl.h ../webtpl.o
	cc -g -O0 -o webtpl_test webtpl_test.c -I.. ../webtpl.o

runtest:	webtpl_test
	@QUERY_STRING="arg1=ARG1&arg2=aaaa&arg3=ARG3&arg2=bbbb&arg2=cccc" ./webtpl_test > test.out
	@diff test.out test.out.std 


clean:	
	rm -f webtpl_test *.o test.out
