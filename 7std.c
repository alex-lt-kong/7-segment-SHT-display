// This is a brute-force translation of https://github.com/shrikantpatnaik/Pi7SegPy/blob/master/Pi7SegPy.py

#include <pigpio.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <linux/i2c-dev.h>

#define DIGIT_COUNT 8

const int data = 22;  // a.k.a. DI0
const int clk = 11;   // a.k.a SCLK
const int latch = 18; // a.k.a. RCLK
const int chain = 2;  // a.k.a. the number of registers (the model we use have two 595 chips, thus we have two pairs of registers)
/*
The 595 has two registers, each with just 8 bits of data. The first one is called the Shift Register.

Whenever we apply a clock pulse to a 595, two things happen:
  * The bits in the Shift Register move one step to the left. For example, Bit 7 accepts the value that was previously
    in bit 6, bit 6 gets the value of bit 5 etc.
  * Bit 0 in the Shift Register accepts the current value on DATA pin.

On enabling the Latch pin, the contents of Shift Register are copied into the second register,
called the Storage/Latch Register. Each bit of the Storage Register is connected to one of the
output pins QA–QH of the IC, so in general, when the value in the Storage Register changes, so do the outputs.
*/

struct SensorPayload {
  double temp_celsius;
  double temp_fahrenheit;
  double humidity;
};  

int get_readings(struct SensorPayload* pl) {
  	// Create I2C bus
	int file;
	char *bus = "/dev/i2c-1";
	if((file = open(bus, O_RDWR)) < 0) 
	{
		printf("Failed to open the bus. \n");
		exit(1);
	}
	// Get I2C device, SHT31 I2C address is 0x44(68)
	ioctl(file, I2C_SLAVE, 0x44);
 
	// Send high repeatability measurement command
	// Command msb, command lsb(0x2C, 0x06)
	char config[2] = {0};
	config[0] = 0x2C;
	config[1] = 0x06;
	write(file, config, 2);

	// Read 6 bytes of data
	// temp msb, temp lsb, temp CRC, humidity msb, humidity lsb, humidity CRC
	char data[6] = {0};
	if(read(file, data, 6) != 6){
		fprintf(stderr, "Error : Input/output Error \n");
    return 1;
	}	else {
    pl->temp_celsius = (((data[0] * 256) + data[1]) * 175.0) / 65535.0  - 45.0;
    pl->temp_fahrenheit = (((data[0] * 256) + data[1]) * 315.0) / 65535.0 - 49.0;
    pl->humidity = (((data[3] * 256) + data[4])) * 100.0 / 65535.0;
    return 0;
	}
}


void push_bit(int bit) {
    gpioWrite(clk, PI_LOW);
    gpioWrite(data, bit);
    gpioWrite(clk, PI_HIGH);
}

int get_bit(unsigned int value, int n) {
    if (value & (1 << n)) {
        return 1;
    } else {
        return 0 ;
    }
}


void initt() {
    gpioSetMode(data, PI_OUTPUT); //make P0 output
    gpioSetMode(clk, PI_OUTPUT); //make P0 output
    gpioSetMode(latch, PI_OUTPUT); //make P0 output

    gpioWrite(clk, PI_LOW);
    gpioWrite(latch, PI_LOW);
}


void show(uint16_t value) {
    for (int i = 8 * chain - 1; i >= 0; --i) {
      push_bit(get_bit(value, i));
    }
    
}

uint8_t handle_dot(uint8_t value, bool turn_it_on) {
    return turn_it_on ? (value & 0b01111111) : value;
}

uint8_t available_chars[] = {
  // controls on/off of 7-segment led + dot. A bit is 0 means to turn that segment led on.
  0b11000000, // 0
  0b11111001, // 1
  0b10100100, // 2
  0b10110000, // 3
  0b10011001, // 4
  0b10010010, // 5
  0b10000010, // 6
  0b11111000, // 7
  0b10000000, // 8
  0b10010000, // 9
  0b11111111, // empty
};

int main() {
  if (gpioInitialise() < 0) {
    fprintf(stderr, "pigpio initialisation failed\n");
    return 1;
  }
  initt();
  struct SensorPayload* pl = malloc(sizeof(struct SensorPayload*));
  uint8_t vals[DIGIT_COUNT];
  bool with_dots[DIGIT_COUNT] = {0,0,1,0,0,0,1,0};
  unsigned int interval = 256;
  while (1) {
    ++interval;
    if (interval > 128) {
      get_readings(pl);
      int temp_celsius = pl->temp_celsius * 10;
      int humidity = pl->humidity * 10;
      vals[0] = 10;
      vals[1] = temp_celsius % 1000 / 100;
      vals[2] = temp_celsius % 100  / 10;
      vals[3] = temp_celsius % 10;
      vals[4] = humidity % 10000 / 1000;
      if (vals[4] == 0) {
        vals[4] = 10;
      }
      vals[5] = humidity % 1000 / 100;
      vals[6] = humidity % 100  / 10;
      vals[7] = humidity % 10;
      /*printf(
        "temp: %d%d%d.%d, humidity: %d%d%d.%d\n",
        vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7]
      );*/
      interval = 0;
    }
    
    for (int i = 0; i < DIGIT_COUNT; ++i) {
      show(handle_dot(available_chars[vals[i]], with_dots[i]) << 8 | 1 << (DIGIT_COUNT - i - 1));
      // we pass a total of 16 bits to show():
      // 1st byte: controls on/off of 7-segment led + dot. A bit is 0 means to turn that segment led on.
      // 2nd byte: controls which digit the above 7-segment definiton should be applied to.
      gpioWrite(latch, PI_HIGH);
      gpioWrite(latch, PI_LOW);      
      usleep(2000);
    }
  }
  free(pl);
  return 0;
}
