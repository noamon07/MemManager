# Default configuration
CONFIG ?= debug

# Compiler
CC := gcc

# --- PATHS ---
# Adding -Iinclude lets you use #include <header.h> instead of relative paths
COMMON_CFLAGS := -Wall -Werror -Wextra -Iinclude -D_strdup=strdup
LDFLAGS := -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
# --- Configuration Specifics ---
ifeq ($(CONFIG), debug)
    OUTDIR := out/debug
    CFLAGS := $(COMMON_CFLAGS) -g -O0
    TARGET_NAME := main_debug
else
    OUTDIR := out/release
    CFLAGS := $(COMMON_CFLAGS) -O2 -DNDEBUG
    TARGET_NAME := main
endif

# --- RECURSIVE SOURCE DISCOVERY ---
# This finds all .c files in src/ and its subfolders (intelligence, decision, etc.)
SRCS := $(shell find src -name '*.c')

# This keeps the folder structure inside your /out/obj directory
OBJDIR := $(OUTDIR)/obj
OBJS := $(SRCS:%.c=$(OBJDIR)/%.o)
TARGET := $(OUTDIR)/$(TARGET_NAME)

# --- TARGETS ---
.PHONY: all clean debug release

all: $(TARGET)

debug:
	@$(MAKE) all CONFIG=debug

release:
	@$(MAKE) all CONFIG=release

$(TARGET): $(OBJS)
	@mkdir -p $(shell dirname $@)
	@echo "Linking target: $@"
	@$(CC) $^ -o $@ $(LDFLAGS)

# The % pattern here now handles paths like src/intelligence/telemetry.o
$(OBJDIR)/%.o: %.c
	@mkdir -p $(shell dirname $@)
	@echo "Compiling: $< -> $@"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "Cleaning up..."
	@rm -rf out

rebuild: clean all
	@echo "Rebuilding..."

run: all
	@echo "Running $(TARGET)..."
	@./$(TARGET)