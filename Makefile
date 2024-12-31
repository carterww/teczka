TARGET = teczka

CC = gcc
CFLAGS = -Werror -std=c11 -Wpedantic -Wall -Wextra -Wno-unused -Wfloat-equal -Wdouble-promotion -Wformat-overflow=2 -Wformat=2 -Wnull-dereference -Wno-unused-result -Wmissing-include-dirs -Wswitch-default -Wswitch-enum

OBJ = main.o event.o portfolio.o static_mem_cache.o portfolio_import.o curl_callbacks.o util.o
OBJ_OUT = $(patsubst %, build/%, $(OBJ))

LINK_LIBS = -lcurl -lrt

all: build bin $(OBJ_OUT)
	$(CC) -o bin/$(TARGET) $(OBJ_OUT) $(LINK_LIBS)

build/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

build:
	@mkdir -p build
bin:
	@mkdir -p bin
clean:
	rm -rf build bin

.PHONY: all build bin clean
