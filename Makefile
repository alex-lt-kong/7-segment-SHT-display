main: 7std.c pi_2_dht_read.o pi_2_mmio.o common_dht_read.o
	gcc 7std.c pi_2_dht_read.o pi_2_mmio.o common_dht_read.o -o 7std.out -lpigpio -lpthread -I./libs/
	gcc test.c -o test.out

pi_2_dht_read.o: ./libs/pi_2_dht_read.c ./libs/pi_2_dht_read.h
	gcc -c ./libs/pi_2_dht_read.c

pi_2_mmio.o: ./libs/pi_2_mmio.c ./libs/pi_2_mmio.h
	gcc -c ./libs/pi_2_mmio.c

common_dht_read.o: ./libs/common_dht_read.c ./libs/common_dht_read.h
	gcc -c ./libs/common_dht_read.c


clean:
	rm *.out *.o