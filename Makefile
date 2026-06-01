CC        = gcc
CFLAGS    = -std=c11 -Wall -Wextra -Wpedantic -g -fsanitize=address,undefined

CORE_OBJS = fel.o clock.o context.o kernel.o
HEADERS   = fel.h event.h context.h kernel.h
TESTS     = test_fel test_clock

# Disable ASLR per-process.
# Workaround for intermittent ASan startup crashes on this WSL2 system.
NOASLR = setarch $(shell uname -m) -R

all: $(TESTS)

test: $(TESTS)
	$(NOASLR) ./test_fel
	$(NOASLR) ./test_clock

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

test_fel: test_fel.o $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

test_clock: test_clock.o $(CORE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o $(TESTS)

.PHONY: all test clean