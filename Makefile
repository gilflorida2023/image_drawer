# Makefile for image_drawer (with TTF and .vd input)

CC = gcc
CFLAGS = -Wall -g -I/usr/include/SDL2 -D_REENTRANT $(sdl2_image_cflags) $(sdl2_ttf_cflags) # Include paths for SDL2, necessary defines, plus image and ttf cflags
LDFLAGS = -lSDL2 -lSDL2_image -lSDL2_ttf -lm # Explicitly listing SDL2, SDL2_image, SDL2_ttf, and Math libraries

TARGET = image_drawer
SOURCES = image_drawer.c

# Try pkg-config for CFLAGS, with fallback if it fails
sdl2_image_cflags := $(shell pkg-config --cflags SDL2_image || echo "")
sdl2_ttf_cflags := $(shell pkg-config --cflags SDL2_ttf || echo "")


.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) *.png
