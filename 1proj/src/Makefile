CC=gcc
CFLAGS=--std=gnu99 -Wall -Wextra -pedantic
LDFLAGS=-L . -lm -lsndfile


all: bms1A bms1B

bms1A: bms1A.c
	$(CC) $(CFLAGS) $(^) -o $(@) $(LDFLAGS)

bms1B: bms1B.c
	$(CC) $(CFLAGS) $(^) -o $(@) $(LDFLAGS)


clean:
	rm -f bms1A bms1B
