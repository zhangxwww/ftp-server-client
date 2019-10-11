server: server.o utils.o errors.o handlers.o
	gcc server.o utils.o errors.o handlers.o -o server -Wall

handlers.o: handlers.c handlers.h errors.h definition.h utils.h
	gcc -c handlers.c -o handlers.o -Wall

errors.o: errors.c errors.h definition.h
	gcc -c errors.c -o errors.o -Wall

utils.o: utils.c utils.h definition.h errors.h
	gcc -c utils.c -o utils.o -Wall

server.o: server.c definition.h errors.h utils.h
	gcc -c server.c -o server.o -Wall

.PHONY: clean
clean:
	rm server
	rm *.o
