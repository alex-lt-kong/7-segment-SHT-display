main: 7std.c
	gcc 7std.c -o 7std.out -lpigpio -lpthread

clean:
	rm *.out