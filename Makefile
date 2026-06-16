CC        = gcc
CFLAGS    = -std=c11 -Wall -Wextra -Wpedantic -g -fsanitize=address,undefined

CORE_OBJS = fel.o clock.o context.o kernel.o
HEADERS   = fel.h event.h context.h kernel.h entity.h entity_queue.h mm1_state.h mm1_handlers.h mm1_init.h time_acc.h sample_acc.h mm1_report.h
TESTS     = test_fel test_clock test_dispatch test_prng_dist test_mm1

BUILD_DIR = build
OBJS      = $(patsubst %.o,$(BUILD_DIR)/%.o,\
            $(CORE_OBJS) \
            test_fel.o test_clock.o test_dispatch.o test_prng_dist.o test_mm1.o \
            dispatch.o prng.o dist.o entity.o entity_queue.o mm1_handlers.o mm1_init.o time_acc.o sample_acc.o mm1_report.o )
BINS      = $(patsubst %,$(BUILD_DIR)/%,$(TESTS))

# Disable ASLR per-process. My WSL spec :<
NOASLR = setarch $(shell uname -m) -R

all: $(BINS)

test: $(BINS)
	$(NOASLR) ./$(BUILD_DIR)/test_fel
	$(NOASLR) ./$(BUILD_DIR)/test_clock
	$(NOASLR) ./$(BUILD_DIR)/test_dispatch
	$(NOASLR) ./$(BUILD_DIR)/test_prng_dist
	$(NOASLR) ./$(BUILD_DIR)/test_mm1

# create build dir
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# object files
$(BUILD_DIR)/%.o: %.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# stochastic layer
$(BUILD_DIR)/prng.o: prng.c prng.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/dist.o: dist.c dist.h prng.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


$(BUILD_DIR)/time_acc.o: time_acc.c time_acc.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sample_acc.o: sample_acc.c sample_acc.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@ -lm

$(BUILD_DIR)/mm1_report.o: mm1_report.c mm1_report.h mm1_state.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Specific target rules
$(BUILD_DIR)/test_fel: $(BUILD_DIR)/test_fel.o $(patsubst %.o,$(BUILD_DIR)/%.o,$(CORE_OBJS))
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/test_clock: $(BUILD_DIR)/test_clock.o $(patsubst %.o,$(BUILD_DIR)/%.o,$(CORE_OBJS))
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/test_dispatch: $(BUILD_DIR)/test_dispatch.o $(BUILD_DIR)/dispatch.o $(patsubst %.o,$(BUILD_DIR)/%.o,$(CORE_OBJS))
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BUILD_DIR)/test_prng_dist: $(BUILD_DIR)/test_prng_dist.o \
                             $(BUILD_DIR)/prng.o \
                             $(BUILD_DIR)/dist.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

$(BUILD_DIR)/test_mm1: $(BUILD_DIR)/test_mm1.o \
                             $(BUILD_DIR)/entity.o $(BUILD_DIR)/entity_queue.o $(BUILD_DIR)/mm1_handlers.o $(BUILD_DIR)/mm1_init.o \
                             $(BUILD_DIR)/prng.o $(BUILD_DIR)/dist.o \
                             $(patsubst %.o,$(BUILD_DIR)/%.o,$(CORE_OBJS)) \
                             $(BUILD_DIR)/dispatch.o \
							 $(BUILD_DIR)/time_acc.o \
							 $(BUILD_DIR)/sample_acc.o \
    						 $(BUILD_DIR)/mm1_report.o 
	$(CC) $(CFLAGS) -o $@ $^ -lm

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all test clean