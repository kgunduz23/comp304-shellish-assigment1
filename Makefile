CC=gcc
CFLAGS=-Wall -Wextra -g
TARGET=shellish
SRC=shellish-skeleton.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

asan: $(SRC)
	$(CC) -g -O0 -fsanitize=address -fno-omit-frame-pointer $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o

.PHONY: all asan clean
