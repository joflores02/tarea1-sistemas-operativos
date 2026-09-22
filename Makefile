CC      = gcc
CFLAGS  = -Wall -Wextra -std=gnu11 -DUSE_READLINE
LDLIBS  = -lreadline
SRC     = src/mishell.c
TARGET  = mishell

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)