CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -g -fsanitize=address,undefined

TARGET  = test_fel
SRC     = fel.c test_fel.c

all: $(TARGET)

$(TARGET): $(SRC) fel.h event.h
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
