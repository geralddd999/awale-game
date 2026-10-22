CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -Wshadow -g
LDFLAGS =

BIN_DIR = bin

SERVER_SRC = $(wildcard src/server/*.c)
SERVER_HDR = $(wildcard src/server/*.h)
CLIENT_SRC = $(wildcard src/client/*.c)
CLIENT_HDR = $(wildcard src/client/*.h)

all: $(BIN_DIR)/server $(BIN_DIR)/client

$(BIN_DIR)/server: $(SERVER_SRC) $(SERVER_HDR) | $(BIN_DIR)
	$(CC) $(CFLAGS) -Isrc/server $(SERVER_SRC) -o $@ $(LDFLAGS)

$(BIN_DIR)/client: $(CLIENT_SRC) $(CLIENT_HDR) | $(BIN_DIR)
	$(CC) $(CFLAGS) -Isrc/client $(CLIENT_SRC) -o $@ $(LDFLAGS)

$(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BIN_DIR)

.PHONY: all clean
