run: run.c pipe.o include.o socket.o
	gcc run.c include.o pipe.o socket.o -g -Wall -Werror

pipe.o: pipe.c include.o
	gcc pipe.c -c -Wall -Werror

socket.o: socket.c include.o
	gcc socket.c -c -Wall -Werror

include.o: include.c
	gcc include.c -c -Wall -Werror

clean:
	rm run
	rm include
	rm pipe