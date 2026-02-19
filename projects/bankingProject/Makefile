######################################
# Project settings
######################################

TARGET      := app
SRC_DIR     := src
OBJ_DIR     := obj
BIN_DIR     := bin

######################################
# Toolchain
######################################

CC          := gcc
CXX         := g++
CFLAGS      := -Wall -Wextra -Werror -std=c11 -g
CXXFLAGS    := -Wall -Wextra -Werror -std=c++17 -g
LDFLAGS     :=

######################################
# Source files
######################################

SRCS        := $(wildcard $(SRC_DIR)/*.c)
OBJS        := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))
DEPS        := $(OBJS:.o=.d)

######################################
# Default target
######################################

.PHONY: all
all: $(BIN_DIR)/$(TARGET)

######################################
# Link
######################################

$(BIN_DIR)/$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

######################################
# Compile
######################################

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

######################################
# Directories
######################################

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

######################################
# Utility targets
######################################

.PHONY: clean run debug

clean:
	rm -f $(OBJ_DIR)/*.o $(BIN_DIR)/*

run: all
	./$(BIN_DIR)/$(TARGET)

debug:
	$(MAKE) CFLAGS="$(CFLAGS) -DDEBUG"

######################################
# Dependencies
######################################

-include $(DEPS)

