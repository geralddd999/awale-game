CC = gcc
CFLAGS = -Wall -Wextra -g -Isrc -Isrc/common
BUILD = build

GAME_SRC   = src/game.c src/board_view.c
COMMON_SRC = $(wildcard src/common/*.c)
SERVER_SRC = $(wildcard src/server/*.c)
CLIENT_SRC = $(wildcard src/client/*.c)
HEADERS    = $(wildcard src/*.h src/common/*.h src/server/*.h src/client/*.h)

.PHONY: all test e2e play clean

all: $(BUILD)/server $(BUILD)/client

$(BUILD)/server: $(SERVER_SRC) $(COMMON_SRC) $(GAME_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(COMMON_SRC) $(GAME_SRC)

$(BUILD)/client: $(CLIENT_SRC) $(COMMON_SRC) $(GAME_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC) $(COMMON_SRC) $(GAME_SRC)

test: $(BUILD)/test_game $(BUILD)/test_linebuf
	./$(BUILD)/test_game
	./$(BUILD)/test_linebuf

e2e: all
	./tests/e2e.sh

play: $(BUILD)/play_local
	./$(BUILD)/play_local

$(BUILD)/test_game: tests/test_game.c src/game.c src/game.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_game.c src/game.c

$(BUILD)/test_linebuf: tests/test_linebuf.c $(COMMON_SRC) src/common/net.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/test_linebuf.c $(COMMON_SRC)

$(BUILD)/play_local: tests/play_local.c $(GAME_SRC) src/game.h src/board_view.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ tests/play_local.c $(GAME_SRC)

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
