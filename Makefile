CC = gcc
CFLAGS = -Wall -Wextra -g -Isrc
BUILD = build

all: test

test: $(BUILD)/test_game
	./$(BUILD)/test_game

play: $(BUILD)/play_local
	./$(BUILD)/play_local

$(BUILD)/test_game: tests/test_game.c src/game.c src/game.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_game.c src/game.c

$(BUILD)/play_local: tests/play_local.c src/game.c src/board_view.c src/game.h src/board_view.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/play_local.c src/game.c src/board_view.c
.PHONY: all test play clean

SERVER_SRC = $(wildcard src/server/*.c)
SERVER_HDR = $(wildcard src/server/*.h)
CLIENT_SRC = $(wildcard src/client/*.c)
CLIENT_HDR = $(wildcard src/client/*.h)

all: $(BUILD)/server $(BUILD)/client

$(BUILD)/server: $(SERVER_SRC) $(SERVER_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -Isrc/server $(SERVER_SRC) -o $@

$(BUILD)/client: $(CLIENT_SRC) $(CLIENT_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -Isrc/client $(CLIENT_SRC) -o $@

$(BUILD):
	mkdir -p $@


clean:
	rm -rf $(BUILD)
