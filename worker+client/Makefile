# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread
TARGET = node

# Node.js setup
NPM = npm
NODE = node

# Default target: Build the C node
all: $(TARGET)

# Compile the C node
$(TARGET): server.c
	$(CC) $(CFLAGS) server.c -o $(TARGET)
	@echo "Built C Node: $(TARGET)"

# Install Node.js dependencies
install:
	$(NPM) install express multer
	@echo "Node.js dependencies installed."

# Run the C node (Background Mode)
run-node: $(TARGET)
	./$(TARGET)

# Run the Web Interface
run-web: install
	$(NODE) server.js

# Clean up binaries and temporary files
clean:
	rm -f $(TARGET)
	rm -f incoming_*.c exec_*
	rm -rf uploads/*
	@echo "Cleaned up cluster files."

# Help command
help:
	@echo "Usage:"
	@echo "  make            - Compile the C node"
	@echo "  make run-node   - Start the C node in background mode"
	@echo "  make run-web    - Install JS deps and start Web UI"
	@echo "  make clean      - Remove binaries and temp task files"

.PHONY: all install run-node run-web clean help