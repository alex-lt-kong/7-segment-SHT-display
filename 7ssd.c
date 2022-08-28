// This is a brute-force translation of https://github.com/shrikantpatnaik/Pi7SegPy/blob/master/Pi7SegPy.py

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <pigpio.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <syslog.h>

#define DIGIT_COUNT 8

volatile sig_atomic_t done = 0;

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
  bool success;
};

void signal_handler(int signum) {
  syslog(LOG_INFO, "Signal %d received by signal_handler()\n", signum);
  done = 1;
}

void* thread_get_sensor_readings(void* payload) {
  	// Create I2C device_path
	uint32_t fd;
  struct SensorPayload* pl = (struct SensorPayload*)payload;
	char *device_path = "/dev/i2c-1";
  while (!done) {
    for (int i = 0; i < 3; ++i) { // per some specs sheet online, the frequency of DHT31 is 1hz.
      sleep(1); 
      if (done) {break;}
    }
    if((fd = open(device_path, O_RDWR)) < 0) {
      syslog(LOG_ERR, "Failed to open() device_path [%s], this reading attempt will be skipped.\n", device_path);
      continue;
    }
    
    ioctl(fd, I2C_SLAVE, 0x44); // Get I2C device, SHT31 I2C address is 0x44(68)
  
    // Send high repeatability measurement command
    // Command msb, command lsb(0x2C, 0x06)
    uint8_t config[2] = {0x2C, 0x06};
    if (write(fd, config, 2) != 2) {
      syslog(LOG_ERR, "Failed to write() command to [%s], this reading attempt will be skipped.\n", device_path);
      close(fd);
      continue;
    }

    // Read 6 bytes of data
    // temp msb, temp lsb, temp CRC, humidity msb, humidity lsb, humidity CRC
    char data[6] = {0};
    if(read(fd, data, 6) != 6){
      syslog(LOG_ERR, "Failed to read() values from [%s], this reading attempt will be skipped.\n", device_path);
      pl->success = false;
    }	else {
      pl->temp_celsius = (((data[0] * 256) + data[1]) * 175.0) / 65535.0  - 45.0;
      pl->temp_fahrenheit = (((data[0] * 256) + data[1]) * 315.0) / 65535.0 - 49.0;
      pl->humidity = (((data[3] * 256) + data[4])) * 100.0 / 65535.0;
      pl->success = true;
    }
    close(fd);
  }
  syslog(LOG_INFO, "Stop signal received, thread_get_sensor_readings() quits gracefully\n");
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


void init_7seg_display() {
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

int main(int argc, char **argv) {
  openlog("7ssd.out", LOG_PID | LOG_CONS, 0);
  syslog(LOG_INFO, "7ssd.out started\n", argv[0]);

  if (gpioInitialise() < 0) {
    syslog(LOG_ERR, "pigpio initialization failed, program will quit\n");
    closelog();
    return 1;
  }

  struct sigaction act;
  act.sa_handler = signal_handler;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &act, 0);
  sigaction(SIGABRT, &act, 0);
  sigaction(SIGTERM, &act, 0);
  
  init_7seg_display();
  
  struct SensorPayload pl;
  pl.humidity = 0;
  pl.temp_celsius = 0;
  pl.temp_fahrenheit = 0;
  pl.success = false;
  
  pthread_t tid;
  if (pthread_create(&tid, NULL, thread_get_sensor_readings, &pl) != 0) {
    syslog(LOG_ERR, "Failed to create thread_get_sensor_readings() thread, program will quit\n");
    closelog();
    return 1;
  }
  uint8_t vals[DIGIT_COUNT];
  bool with_dots[DIGIT_COUNT] = {0,0,1,0,0,0,1,0};
  unsigned int interval = 256;

  while (!done) {
    ++interval;
    if (interval > 16 && pl.success == true) {
      int temp_celsius = pl.temp_celsius * 10;
      int humidity = pl.humidity * 10;
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
      interval = 0;
    }
    
    for (int i = 0; i < DIGIT_COUNT; ++i) {
      show(handle_dot(available_chars[vals[i]], with_dots[i]) << 8 | 1 << (DIGIT_COUNT - i - 1));
      // we pass a total of 16 bits to show():
      // 1st byte: controls on/off of 7-segment led + dot. A bit is 0 means to turn that segment led on.
      // 2nd byte: controls which digit the above 7-segment definiton should be applied to.
      gpioWrite(latch, PI_HIGH);
      gpioWrite(latch, PI_LOW);      
      usleep(500);
    }
  }
  pthread_join(tid, NULL);
  syslog(LOG_INFO, "Program quits gracefully\n");
  closelog();
  return 0;
}
