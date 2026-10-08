CC=gcc
CFLAGS=-Wall -Wextra -std=c11
TARGET=shellforge_m1

all: $(TARGET)

$(TARGET): main.c history.c history.h
	$(CC) $(CFLAGS) main.c history.c -lreadline -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
