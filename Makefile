CC      = gcc
CFLAGS  = -O2 -std=gnu89 -Wall -Wno-unused-function

tank: tank.c
	$(CC) $(CFLAGS) tank.c -o tank

run: tank
	./tank

clean:
	rm -f tank

.PHONY: run clean
