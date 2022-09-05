main: 7ssd.c
	gcc 7ssd.c -o 7ssd.out -lpigpio -lpthread -lcurl -O2

clean:
	rm *.out *.o