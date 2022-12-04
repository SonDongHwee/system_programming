CC = gcc
CFLAGS = -Wall -Wextra -Werror -Wshadow -pedantic

all: snudbg

snudbg: snudbg.c procmaps.c
	$(CC) -g -o $@ $^ $(CFLAGS)

test: snudbg
	./snudbg ../test/prebuilt/rand < ../test/prebuilt/rand.mem-write.input
	@echo "----------------------------------------------"
	./snudbg ../test/prebuilt/rand < ../test/prebuilt/rand.set-reg.input
	@echo "----------------------------------------------"
	./snudbg ../test/prebuilt/array < ../test/prebuilt/array.check.input
clean:
	rm -f snudbg

