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

clean:
	rm -rf $(BUILD)

.PHONY: all test play clean
