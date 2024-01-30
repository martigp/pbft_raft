SRC_DIR = ./src
BLD_DIR = ./build
ODIR = $(BLD_DIR)/obj

CC = gcc
CCX = g++

SOURCES=$(wildcard $(SRC_DIR)/*.c)
OBJECTS=$(patsubst %.c, %.o, $(SOURCES))

CFLAGS += -Wall -Werror -std=c++17

CFLAGS += $(shell export PKG_CONFIG_PATH=../install/lib/pkgconfig; \
			 pkg-config --cflags libconfig++)

LIBS := $(shell export PKG_CONFIG_PATH=../install/lib/pkgconfig; \
		  pkg-config --cflags --libs libconfig++)

# -o specifies the output fiule
all: server

server: $(SRC_DIR)/RaftServer/*.cc \
		$(SRC_DIR)/Common/*.cc
	$(CCX) $(CFLAGS) $(LIBS) -I $(SRC_DIR) $^ -o $(BLD_DIR)/$@
	export DYLD_LIBRARY_PATH=../install/runtime_libs/

client: $(SRC_DIR)/RaftClient/client.cc $(SRC_DIR)/Common/*.cc
	$(CCX) $(CFLAGS) $(LIBS) -I $(SRC_DIR) $^ -o $(BLD_DIR)/$@

clean:
	rm -rf $(BLD_DIR)/*


