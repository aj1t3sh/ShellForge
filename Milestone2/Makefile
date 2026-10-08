CC=gcc
CFLAGS=-Wall -Wextra -std=c11 -D_GNU_SOURCE
TARGET=shellforge_m2
SRC=main.c token.c parser.c expand.c

all: $(TARGET)
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -lreadline -o $(TARGET)
clean:
	rm -f $(TARGET)
.PHONY: all clean
