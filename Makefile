TARGET = teczka

CC = gcc
CFLAGS = -Werror -std=c11 -Wpedantic -Wall -Wextra -Wno-unused -Wfloat-equal -Wdouble-promotion -Wformat-overflow=2 -Wformat=2 -Wnull-dereference -Wno-unused-result -Wmissing-include-dirs -Wswitch-default -Wswitch-enum

OBJ = main.o event.o portfolio.o static_mem_cache.o portfolio_import.o
OBJ_OUT = $(patsubst %, build/%, $(OBJ))

all: build bin $(OBJ_OUT)
	$(CC) -o bin/$(TARGET) $(OBJ_OUT)

build/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

build:
	@mkdir -p build
bin:
	@mkdir -p bin
clean:
	rm -rf build bin

.PHONY: all build bin clean
