######################################
# Toolchain
######################################
CC     := gcc
CFLAGS := -Wall -Wextra -Werror -std=c23 -g

######################################
# Directories
######################################
BIN_DIR := bin

######################################
# Discover sources / programs
######################################
SRCS := $(wildcard *.c)
PROGS := $(basename $(SRCS))                    # hello.c -> hello
BINS := $(addprefix $(BIN_DIR)/,$(PROGS))       # hello  -> bin/hello

######################################
# Default: build everything
######################################
.PHONY: all
all: $(BINS)

######################################
# "make signal" builds bin/signal
# This line makes each program name a phony alias target.
######################################
.PHONY: $(PROGS)
$(PROGS): %: $(BIN_DIR)/%

######################################
# Build rule: bin/<name> from <name>.c
######################################
$(BIN_DIR)/%: %.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

######################################
# Create bin/
######################################
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

######################################
# Utilities
######################################
.PHONY: clean list
clean:
	rm -rf $(BIN_DIR)

list:
	@echo "Programs:" $(PROGS)

