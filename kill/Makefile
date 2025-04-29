# Compiler and flags
CC = gcc
CFLAGS = -std=c11 -I./libs/include
LDFLAGS = -L./libs -liniparser

# Source files and target
SRCS = asmjs.c tcpx.c main.c
OBJS = $(SRCS:.c=.o)
TARGET = testkill

# Output directory
OUTPUT_DIR = $(HOME)/lubeman

# Default target
all: $(OUTPUT_DIR)/$(TARGET)

# Create the output directory if it doesn't exist
$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

# Build the target
$(OUTPUT_DIR)/$(TARGET): $(OBJS) | $(OUTPUT_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(OBJS) $(OUTPUT_DIR)/$(TARGET)

# Print the home directory (optional, for debugging)
print-home:
	@echo "Home directory: $(HOME)"
