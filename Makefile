server: server.o
	gcc -pthread -Wall -o server server.o 

server.o: server.c server.h 
	gcc -pthread -Wall -c server.c -o server.o

clean:
	rm -f *.o server
	
