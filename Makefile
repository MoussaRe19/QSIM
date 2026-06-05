CC        = gcc
CFLAGS    = -std=c11 -Wall -Wextra -Wpedantic -g -fsanitize=address,undefined

CORE_OBJS = fel.o clock.o context.o kernel.o
HEADERS   = fel.h event.h context.h kernel.h
TESTS     = test_fel test_clock test_dispatch

BUILD_DIR = build
OBJS      = $(patsubst %.o,$(BUILD_DIR)/%.o,$(CORE_OBJS) test_fel.o test_clock.o test_dispatch.o dispatch.o)
BINS      = $(patsubst %,$(BUILD_DIR)/%,$(TESTS))

# Disable ASLR per-process. My WSL spec :<
NOASLR = setarch $(shell uname -m) -R

all: $(BINS)

test: $(BINS)
	$(NOASLR) ./$(BUILD_DIR)/test_fel
	$(NOASLR) ./$(BUILD_DIR)/test_clock
	$(NOASLR) ./$(BUILD_DIR)/test_dispatch

# create build dir
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# object files
$(BUILD_DIR)/%.o: %.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Specific target rules
$(BUILD_DIR)/test_fel: $(BUILD_DIR)/test_fel.o $(patsubst %.o,$(BUILD_DIR)/%.o,$(CORE_OBJS))
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/test_clock: $(BUILD_DIR)/test_clock.o $(patsubst %.o,$(BUILD_DIR)/%.o,$(CORE_OBJS))
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/test_dispatch: $(BUILD_DIR)/test_dispatch.o $(BUILD_DIR)/dispatch.o $(patsubst %.o,$(BUILD_DIR)/%.o,$(CORE_OBJS))
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all test clean