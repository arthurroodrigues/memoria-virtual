CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread

vm: vm.c
	$(CC) $(CFLAGS) -o vm vm.c

clean:
	rm -f vm

.PHONY: clean