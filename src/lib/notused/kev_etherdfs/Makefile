#
# ethersrv-linux makefile for Linux (GCC)
# http://etherdfs.sourceforge.net
#
# Copyright (C) 2017, 2018 Mateusz Viste
#

CFLAGS = -O2 -Wall -std=gnu89 -pedantic -Wextra -s -Wno-long-long -Wno-variadic-macros -Wformat-security -D_FORTIFY_SOURCE=1

CC = gcc

ethersrv-linux: ethersrv-linux.c fs.c fs.h lock.c lock.h debug.h
	$(CC) ethersrv-linux.c fs.c lock.c -o ethersrv-linux $(CFLAGS)

clean:
	rm -f ethersrv-linux *.o
