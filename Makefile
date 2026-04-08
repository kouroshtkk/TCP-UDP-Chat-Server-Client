Compiler = gcc
CFLAGS = -Wall -pthread -g
all: direct server client
direct:
	mkdir -p bin

server: server.c protocol.h
	$(Compiler) $(CFLAGS) server.c -o bin/server
client: client.c protocol.h
	$(Compiler) $(CFLAGS) client.c -o bin/client
clean:	
	rm -rf *~ ./bin
rebuild: clean all
