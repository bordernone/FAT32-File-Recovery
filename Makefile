CC=gcc
CFLAGS=-g -pedantic -std=gnu17 -Wall -Wextra -Werror
LDFLAGS=-lm -lssl -lcrypto

.PHONY: all
all: nyufile

nyufile: nyufile.o helper.o core.o

nyufile.o: nyufile.c fat32_struct.h helper.h core.h common.h

core.o: core.c core.h common.h

helper.o: helper.c helper.h common.h

.PHONY: clean
clean:
	rm -f *.o nyufile
