run: run.c pipe.o include.o
	gcc run.c include.o pipe.o -Wall -Werror

pipe.o: pipe.c include.o
	gcc pipe.c -c -Wall -Werror

include.o: include.c
	gcc include.c -c -Wall -Werror

clean:
	rm run
	rm include
	rm pipe