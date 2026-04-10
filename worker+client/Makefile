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

# Install Node.js dependencies (Added cors)
install:
	$(NPM) install express multer cors
	@echo "Node.js dependencies installed."

# Run the C node
run-node: $(TARGET)
	./$(TARGET)

# Run the Web Interface
run-web:
	$(NODE) server.js

# Clean up binaries and temporary files (Updated file names)
clean:
	rm -f $(TARGET)
	rm -f incoming_task.c task_bin compile_errors.log
	rm -rf uploads/*
	@echo "Cleaned up cluster files."

# Help command
help:
	@echo "Usage:"
	@echo "  make            - Compile the C node"
	@echo "  make run-node   - Start the C node"
	@echo "  make run-web    - Start Web UI (Assumes install done)"
	@echo "  make install    - Install JS dependencies (express, multer, cors)"
	@echo "  make clean      - Remove binaries and temp task files"

.PHONY: all install run-node run-web clean help