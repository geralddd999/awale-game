CC = gcc
CFLAGS = -Wall -Wextra -g -Isrc -Isrc/common
BUILD = build

NCURSES_CFG := $(shell which ncursesw6-config ncurses6-config 2>/dev/null | head -n 1)
ifeq ($(NCURSES_CFG),)
NCURSES_DEV := $(shell find /nix/store -maxdepth 1 -name "*-ncurses-*-dev" 2>/dev/null | head -n 1)
ifneq ($(NCURSES_DEV),)
NCURSES_CFG := $(NCURSES_DEV)/bin/ncursesw6-config
endif
endif

NCURSES_CFLAGS ?= $(shell $(NCURSES_CFG) --cflags 2>/dev/null || pkg-config --cflags ncursesw 2>/dev/null || pkg-config --cflags ncurses 2>/dev/null)
NCURSES_LIBS   ?= $(shell $(NCURSES_CFG) --libs 2>/dev/null || pkg-config --libs ncursesw 2>/dev/null || pkg-config --libs ncurses 2>/dev/null || echo "-lncurses")

GAME_SRC   = src/game.c src/board_view.c
COMMON_SRC = $(wildcard src/common/*.c)
SERVER_SRC = $(wildcard src/server/*.c)
CLIENT_SRC = $(wildcard src/client/*.c)
HEADERS    = $(wildcard src/*.h src/common/*.h src/server/*.h src/client/*.h)

.PHONY: all test e2e play clean format

all: $(BUILD)/server $(BUILD)/client

$(BUILD)/server: $(SERVER_SRC) $(COMMON_SRC) $(GAME_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(COMMON_SRC) $(GAME_SRC)

$(BUILD)/client: $(CLIENT_SRC) $(COMMON_SRC) $(GAME_SRC) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(NCURSES_CFLAGS) -o $@ $(CLIENT_SRC) $(COMMON_SRC) $(GAME_SRC) $(NCURSES_LIBS)

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

format:
	find src tests -type f \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 clang-format -i
