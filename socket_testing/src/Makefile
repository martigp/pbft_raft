SRC_DIR = ./src
BLD_DIR = ./build

CC = gcc
CCX = g++

CFLAGS += -Wall -Werror -std=c++20

# -o specifies the output fiule
all: server client

proto_middleman: test.proto
	protoc --cpp_out=. $<

server.o: server.cc
	$(CCX) $(CFLAGS) -c $^

client.o: client.cc
	$(CCX) $(CFLAGS) -c $^

server: server.o
	$(CCX) -o $@ $^

client: client.o
	$(CCX) $(LDFLAGS) -o $@ $^

clean:
	rm server.o client.o server client


