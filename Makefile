run: run.c pipe.o include.o socket.o shared_mem.o
	gcc run.c include.o pipe.o socket.o shared_mem.o -g -Wall -Werror -o run

shared_mem.o: shared_mem.c include.o
	gcc shared_mem.c -c -Wall -Werror

pipe.o: pipe.c include.o
	gcc pipe.c -c -Wall -Werror

socket.o: socket.c include.o
	gcc socket.c -c -Wall -Werror

include.o: include.c
	gcc include.c -c -Wall -Werror

clean:
	@rm -f run
	@rm -f include.o
	@rm -f pipe.o
	@rm -f shared_mem.o