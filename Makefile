CC = gcc
LIBS = -lpigpio -lpthread -lcurl
OPTS = -O2 -Wall -pedantic -Wextra -Wc++-compat

main: 7ssd.c
	$(CC) 7ssd.c -o 7ssd.out $(LIBS) $(OPTS)

.PHONY:
clean:
	rm *.out *.o