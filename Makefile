# Makefile for xtrlock - X Transparent Lock
# This Makefile provided for those of you who lack a functioning xmkmf.
#
# Copyright (C)1993,1994 Ian Jackson
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

LDLIBS = -lX11 -lskarnet -lssh2
CC = gcc
CFLAGS = -Wall -Os
LDFLAGS = -s

xtrlock: xtrlock.o

xtrlock.o: xtrlock.c lock.bitmap mask.bitmap patchlevel.h
