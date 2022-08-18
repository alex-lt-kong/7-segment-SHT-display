// This is a brute-force translation of https://github.com/shrikantpatnaik/Pi7SegPy/blob/master/Pi7SegPy.py

#include <pigpio.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <pthread.h>

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
};

void* read_temp(void* temperature) {
  char device_path[] = "/sys/bus/w1/devices/28-01131a3efcd4/w1_slave";
  char buf[256];
  while (1) {
    int fd = open(device_path, O_RDONLY);
    if(fd >= 0) {
      if(read( fd, buf, sizeof(buf) ) > 0) {          
          char* temp_str = strstr(buf, "t=") + 2;
          sscanf(temp_str, "%d", (int16_t*)temperature);
      }
      close(fd);
    } else {
      fprintf(stderr, "Unable to open device at [%s], skipped this read iteration.\n", device_path);
    }
    sleep(60);
  }
}

int main() {
  int16_t temperature = 65535;
  if (gpioInitialise() < 0) {
    fprintf(stderr, "pigpio initialisation failed\n");
    return 1;
  }
  pthread_t id;
  int err = pthread_create(&id, NULL, read_temp, &temperature);
  initt();
  uint8_t vals[4];
  bool with_dots[] = {0,1,0,0};
  unsigned int interval = 256;
  while (1) {
    ++interval;
    if (interval > 128) {
      vals[0] = (temperature % 100000) / 10000;
      vals[1] = (temperature % 10000) / 1000;
      vals[2] = (temperature % 1000) / 100;
      vals[3] = (temperature % 100) / 10;
      // printf("%f: %d%d%d%d\n", temperature / 1000.0, vals[0], vals[1], vals[2], vals[3]);
      interval = 0;
    }

    for (int i = 0; i < 4; ++i) {
      show(handle_dot(available_chars[vals[i]], with_dots[i]) << 8 | 1 << (3-i));
      // we pass a total of 16 bits to show():
      // 1st byte: controls on/off of 7-segment led + dot. A bit is 0 means to turn that segment led on.
      // 2nd byte: controls which digit the above 7-segment definiton should be applied to.
      gpioWrite(latch, PI_HIGH);
      gpioWrite(latch, PI_LOW);      
      usleep(5000);
    }
  }

  return 0;
}
