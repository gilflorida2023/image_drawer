# Makefile for image_drawer (with TTF and .vd input)

CC = gcc
CFLAGS = -Wall -g -I/usr/include/SDL2 -D_REENTRANT 
LDFLAGS = -lSDL2 -lSDL2_image -lSDL2_ttf -lm # Explicitly listing SDL2, SDL2_image, SDL2_ttf, and Math libraries

TARGET = image_drawer
SOURCES = image_drawer.c



.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
