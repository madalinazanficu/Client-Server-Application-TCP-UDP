CC = gcc
CFLAGS =  -Wall -Wextra 

all: server subscriber
# Compileaza server.c
server: server.cpp
	g++ -o server -Wall -Wextra server.cpp functions.cpp -I.
# Compileaza subscriber.c
subscriber: client.cpp
	g++ -o subscriber -Wall -Wextra client.cpp functions.cpp -I.

clean:
	rm -f server subscriber