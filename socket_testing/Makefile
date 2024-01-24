SRC_DIR = ./src
BLD_DIR = ./build
ODIR = $(BLD_DIR)/obj
PROTO_DIR = $(SRC_DIR)/Protobuf

CC = gcc
CCX = g++

SOURCES=$(wildcard $(SRC_DIR)/*.c)
OBJECTS=$(patsubst %.c, %.o, $(SOURCES))

CFLAGS += -Wall -Werror -std=c++17

CFLAGS += $(shell export PKG_CONFIG_PATH=$(PWD)/install/lib/pkgconfig; \
			 pkg-config --cflags protobuf libconfig++)


LIBS := $(shell export PKG_CONFIG_PATH=../install/lib/pkgconfig; \
		  pkg-config --cflags --libs protobuf libconfig++)

# -o specifies the output fiule
all: proto_middleman server client

proto_middleman: $(PROTO_DIR)/*.proto
	protoc -I $(PROTO_DIR) --cpp_out=$(PROTO_DIR) $^

server: $(SRC_DIR)/RaftServer/*.cc $(PROTO_DIR)/test.pb.cc \
		$(SRC_DIR)/Common/*.cc
	$(CCX) $(CFLAGS) $(LIBS) -I $(SRC_DIR) $^ -o $(BLD_DIR)/$@
	export DYLD_LIBRARY_PATH=../install/runtime_libs/

client: $(PROTO_DIR)/test.pb.cc $(SRC_DIR)/RaftClient/client.cc
	$(CCX) $(CFLAGS) $(LIBS) -I $(SRC_DIR) $^ -o $(BLD_DIR)/$@

clean:
	rm -rf $(BLD_DIR)/*


