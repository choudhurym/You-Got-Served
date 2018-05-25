all:	server

server:	project6.c
	gcc -o server project6.c
	
clean:
	rm -f *.o server
