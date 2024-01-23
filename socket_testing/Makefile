SRC_DIR = ./src
BLD_DIR = ./build
ODIR = $(BLD_DIR)/obj
PROTO_DIR = $(SRC_DIR)/protobuf

CC = gcc
CCX = g++

SOURCES=$(wildcard $(SRC_DIR)/*.c)
OBJECTS=$(patsubst %.c, %.o, $(SOURCES))

CFLAGS += -Wall -Werror -std=c++17

CFLAGS += $(shell export PKG_CONFIG_PATH=$(PWD)/install/lib/pkgconfig; \
			 pkg-config --cflags protobuf)


LIBS := $(shell export PKG_CONFIG_PATH=../install/lib/pkgconfig; \
		  pkg-config --cflags --libs protobuf)

# -o specifies the output fiule
all: proto_middleman server client

proto_middleman: $(PROTO_DIR)/*.proto
	protoc -I $(PROTO_DIR) --cpp_out=$(PROTO_DIR) $^

# server.o: $(SRC_DIR)/server.cc
# 	$(CCX) $(CFLAGS) -c $^ -o $(ODIR)/$@

server: $(SRC_DIR)/RaftGlobals.cc $(SRC_DIR)/socket.cc $(SRC_DIR)/server.cc \
		$(PROTO_DIR)/test.pb.cc
	$(CCX) $(CFLAGS) $(LIBS) $^ -o $(BLD_DIR)/$@

# client: $(SRC_DIR)/client.cc
# 	$(CCX) $(CFLAGS) $< -o $(BLD_DIR)/$@

client: proto_middleman $(SRC_DIR)/client.cc
	$(CCX) $(CFLAGS) $(LIBS) $(SRC_DIR)/client.cc $(PROTO_DIR)/test.pb.cc \
					-I ../install/include \
					-o $(BLD_DIR)/$@

# client: client.o
# 	$(CCX) $(LIBS) -o $(BLD_DIR)/$@ $(ODIR)/$^

clean:
	rm -rf $(BLD_DIR)/*


