main: 7ssd.c
	gcc 7ssd.c -o 7ssd.out -lpigpio -lpthread

clean:
	rm *.out *.o