#include "net.h"
#include <stdio.h>
#include <string.h>

static int passed = 0;
static int failed = 0;

#define CHECK(cond) do { \
	if (cond) passed++; \
	else { failed++; printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

static void push(LineBuf *lb, const char *s){
	linebuf_push(lb, s, strlen(s));
}

/* two commands in one recv() must come out as two lines */
static void test_coalesced(void){
	LineBuf lb;
	char line[BUF_SIZE];
	linebuf_init(&lb);
	push(&lb, "/list\n/lobbies\n");

	CHECK(linebuf_next(&lb, line, sizeof line) == 1 && strcmp(line, "/list") == 0);
	CHECK(linebuf_next(&lb, line, sizeof line) == 1 && strcmp(line, "/lobbies") == 0);
	CHECK(linebuf_next(&lb, line, sizeof line) == 0);
	CHECK(lb.len == 0);
}

/* a command split over several recv() is only returned once complete */
static void test_split(void){
	LineBuf lb;
	char line[BUF_SIZE];
	linebuf_init(&lb);

	push(&lb, "/mo");
	CHECK(linebuf_next(&lb, line, sizeof line) == 0);
	push(&lb, "ve 3");
	CHECK(linebuf_next(&lb, line, sizeof line) == 0);
	push(&lb, "\r\n/say");
	CHECK(linebuf_next(&lb, line, sizeof line) == 1 && strcmp(line, "/move 3") == 0);
	CHECK(linebuf_next(&lb, line, sizeof line) == 0);
	CHECK(lb.len == 4 && memcmp(lb.data, "/say", 4) == 0);
}

static void test_empty_line(void){
	LineBuf lb;
	char line[BUF_SIZE];
	linebuf_init(&lb);
	push(&lb, "\n");
	CHECK(linebuf_next(&lb, line, sizeof line) == 1 && line[0] == '\0');
}

static void test_overflow(void){
	LineBuf lb;
	char big[BUF_SIZE];
	linebuf_init(&lb);
	memset(big, 'x', sizeof big);

	CHECK(linebuf_push(&lb, big, sizeof big) == 0);
	CHECK(linebuf_overflow(&lb));
	CHECK(linebuf_push(&lb, "y", 1) == -1);

	/* a full buffer that still contains a newline is not an overflow */
	linebuf_init(&lb);
	big[10] = '\n';
	linebuf_push(&lb, big, sizeof big);
	CHECK(!linebuf_overflow(&lb));
}

int main(void){
	test_coalesced();
	test_split();
	test_empty_line();
	test_overflow();

	printf("linebuf: %d passed, %d failed\n", passed, failed);
	return failed != 0;
}
