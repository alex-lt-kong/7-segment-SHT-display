#CC = gcc
CC = clang
LIBS = -lpigpio -lpthread -lcurl
OPTS = -O2 -Wall -pedantic -Wextra -Wc++-compat

main: 7ssd.c
	$(CC) 7ssd.c -o 7ssd.out $(LIBS) $(OPTS)
#	$(CC) 7ssd.c -fsanitize=address -fno-omit-frame-pointer -g -o 7ssd.out $(OPTS) $(LIBS)

.PHONY:
clean:
	rm *.out *.o
