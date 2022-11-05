# Makefile for xtrlock - X Transparent Lock
#
# Copyright (C)1993,1994 Ian Jackson
#
# See Copyright file for details

LDLIBS = -lX11 -lskarnet -lssh2
CC = gcc
CFLAGS = -Wall -Os -I.
LDFLAGS = -s

xtrlock: xtrlock.o strtonum.o

xtrlock.o: xtrlock.c lock.bitmap mask.bitmap patchlevel.h stdlib.h

strtonum.o: strtonum.c stdlib.h

.PHONY: clean

clean:
	rm -f *.o xtrlock
