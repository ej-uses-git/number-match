OS=$(shell uname -s)

CC=gcc
CFLAGS=$(shell cat compile_flags.txt)
ifeq ($(OS),Darwin)
	CFLAGS+=-framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL
endif
CFLAGS+=-L./thirdparty/raylib/src -lraylib
ifneq ($(BUILD_KIND), RELEASE)
	CFLAGS+=-g -fsanitize=address
endif

.PHONY: all

all: build/main

thirdparty/raylib/src/libraylib.a:
	$(MAKE) -C thirdparty/raylib/src/

build/:
	mkdir -p build

build/main: src/main.c build/ thirdparty/raylib/src/libraylib.a
	$(CC) $(CFLAGS) -o $@ $<
