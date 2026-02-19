######################################
# Layout
######################################
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

######################################
# Toolchain
######################################
CC      := gcc
CFLAGS  := -Wall -Wextra -Werror -std=c11 -g
CPPFLAGS:= -I$(SRC_DIR)

######################################
# Discover programs: one per .c file
######################################
SRCS  := $(wildcard $(SRC_DIR)/*.c)
PROGS := $(basename $(notdir $(SRCS)))          # foo.c -> foo
OBJS  := $(addprefix $(OBJ_DIR)/,$(PROGS:=.o))  # foo -> obj/foo.o
BINS  := $(addprefix $(BIN_DIR)/,$(PROGS))      # foo -> bin/foo
DEPS  := $(OBJS:.o=.d)

######################################
# Default target: build everything
######################################
.PHONY: all
all: $(BINS)

######################################
# Build one program by name:
#   make foo  => builds bin/foo (and obj/foo.o)
######################################
.PHONY: $(PROGS)
$(PROGS): %: $(BIN_DIR)/%

######################################
# Link: bin/foo from obj/foo.o
######################################
$(BIN_DIR)/%: $(OBJ_DIR)/%.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

######################################
# Compile: obj/foo.o from src/foo.c (+ auto header deps)
######################################
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

######################################
# Directories
######################################
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

######################################
# Utilities
######################################
.PHONY: clean list
clean:
	rm -f $(OBJ_DIR)/*.o $(OBJ_DIR)/*.d $(BIN_DIR)/*

list:
	@echo "Programs:" $(PROGS)

######################################
# Dependencies
######################################
-include $(DEPS)

