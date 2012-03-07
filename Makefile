# Makefile for ssl_proxy

CC = g++
FLAGS = -g -fpermissive -Wall
OBJS = ByteBuffer.o ProxyClient.o Client.o ProxyServer.o main.o

all: $(OBJS)
	$(CC) $(FLAGS) bin/*.o -o bin/proxy

ByteBuffer.o: ByteBuffer.cpp
	$(CC) $(FLAGS) -c ByteBuffer.cpp -o bin/$@

ProxyClient.o: ProxyClient.cpp
	$(CC) $(FLAGS) -c ProxyClient.cpp -o bin/$@

Client.o: Client.cpp
	$(CC) $(FLAGS) -c Client.cpp -o bin/$@

ProxyServer.o: ProxyServer.cpp
	$(CC) $(FLAGS) -c ProxyServer.cpp -o bin/$@

main.o: main.cpp
	$(CC) $(FLAGS) -c main.cpp -o bin/$@

clean:
	rm -f *.gch bin/* *~ \#*
