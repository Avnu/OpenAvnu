CC = gcc
LDLIBS = -std=gnu99 -lpcap -lsndfile

simple_listener: simple_listener.c
	$(CC) -o $@ $< $(LDLIBS) $(LDFLAGS)

all: simple_listener

