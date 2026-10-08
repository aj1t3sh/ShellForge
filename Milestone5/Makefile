CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -D_GNU_SOURCE
TARGET=shellforge
SRC=main.c token.c parser.c expand.c builtin.c executor.c jobs.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -lreadline -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
