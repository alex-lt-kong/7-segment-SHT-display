// This is a brute-force translation of https://github.com/shrikantpatnaik/Pi7SegPy/blob/master/Pi7SegPy.py
#include <curl/curl.h>
#include <linux/i2c-dev.h>
#include <limits.h>
#include <fcntl.h>
#include <pthread.h>
#include <pigpio.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>

#include "7seg.c"


volatile sig_atomic_t done = 0;

struct SensorPayload {
  double temp_celsius;
  double temp_fahrenheit;
  double humidity;
  bool success;
};

void signal_handler(int signum) {
  char msg[] = "Signal %d received by signal_handler()";
  syslog(LOG_INFO, msg, signum);
  printf("%s\n", msg);
  done = 1;
}

void* thread_report_sensor_readings(void* payload) {
  syslog(LOG_INFO, "thread_report_sensor_readings() started");
  struct SensorPayload* pl = (struct SensorPayload*)payload;
  char envvar[] = "SEVEN_SSD_TELEMETRY_ENDPOINT";
  if(!getenv(envvar)) {
    syslog(LOG_INFO, "The environment variable [%s] was not found, thread_report_sensor_readings() quits gracefully.", envvar);
    return NULL;
  }
  char telemetry_endpoint[PATH_MAX];
  if(snprintf(telemetry_endpoint, PATH_MAX, "%s", getenv(envvar)) >= PATH_MAX){
    syslog(LOG_INFO, "PATH_MAX too small for %s, thread_report_sensor_readings() quits gracefully.", envvar);
    return NULL;
  }
  uint16_t iter = 0;
  char url[PATH_MAX];
  
  curl_global_init(CURL_GLOBAL_DEFAULT);
  while (!done) {
    sleep(1);
    ++ iter;
    if (iter < 3600) {
      continue;
    }
    iter = 0;
    CURL *curl;
    CURLcode res;
    curl = curl_easy_init();
    if(curl) {
      snprintf(url, PATH_MAX, telemetry_endpoint, pl->temp_celsius);
      curl_easy_setopt(curl, CURLOPT_URL, url);  
      /* Perform the request, res will get the return code */
      res = curl_easy_perform(curl);
      /* Check for errors */
      if(res != CURLE_OK)
        syslog(LOG_ERR, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
  
      /* always cleanup */
      curl_easy_cleanup(curl);
    } else {
      syslog(LOG_ERR, "Failed to create curl instance by curl_easy_init().");
    }
  }
  curl_global_cleanup();
  syslog(LOG_INFO, "Stop signal received, thread_report_sensor_readings() quits gracefully.");
  return NULL;
}

void* thread_get_sensor_readings(void* payload) {
  syslog(LOG_INFO, "thread_get_sensor_readings() started");
	uint32_t fd;
  struct SensorPayload* pl = (struct SensorPayload*)payload;
	char *device_path = "/dev/i2c-1";
  while (!done) {
    for (int i = 0; i < 3; ++i) { // per some specs sheet online, the frequency of DHT31 is 1hz.
      sleep(1); 
      if (done) {break;}
    }
    if((fd = open(device_path, O_RDWR)) < 0) {
      syslog(LOG_ERR, "Failed to open() device_path [%s], this reading attempt will be skipped.", device_path);
      continue;
    }
    
    ioctl(fd, I2C_SLAVE, 0x44); // Get I2C device, SHT31 I2C address is 0x44(68)
  
    // Send high repeatability measurement command
    // Command msb, command lsb(0x2C, 0x06)
    uint8_t config[2] = {0x2C, 0x06};
    if (write(fd, config, 2) != 2) {
      syslog(LOG_ERR, "Failed to write() command to [%s], this reading attempt will be skipped.", device_path);
      close(fd);
      continue;
    }

    // Read 6 bytes of data
    // temp msb, temp lsb, temp CRC, humidity msb, humidity lsb, humidity CRC
    char data[6] = {0};
    if(read(fd, data, 6) != 6){
      syslog(LOG_ERR, "Failed to read() values from [%s], this reading attempt will be skipped.", device_path);
      pl->success = false;
    }	else {
      pl->temp_celsius = (((data[0] * 256) + data[1]) * 175.0) / 65535.0  - 45.0;
      pl->temp_fahrenheit = (((data[0] * 256) + data[1]) * 315.0) / 65535.0 - 49.0;
      pl->humidity = (((data[3] * 256) + data[4])) * 100.0 / 65535.0;
      pl->success = true;
    }
    close(fd);
  }
  syslog(LOG_INFO, "Stop signal received, thread_get_sensor_readings() quits gracefully.");
  return NULL;
}

void* thread_set_7seg_display(void* payload) {
  syslog(LOG_INFO, "thread_set_7seg_display() started.");
  struct SensorPayload* pl = (struct SensorPayload*)payload;
  init_7seg_display();
  uint8_t vals[DIGIT_COUNT];
  bool dots[DIGIT_COUNT] = {0,0,1,0,0,0,1,0};
  int32_t internal_temp;
  uint32_t interval = 0;
  while (!done) {
    ++interval;
    if (interval > 16 && pl->success == true) {
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
      interval = 0;
    }
    show(vals, dots);
  }

  syslog(LOG_INFO, "thread_set_7seg_display() quits gracefully.");
  return NULL;
}

int main(int argc, char **argv) {
  openlog("7ssd.out", LOG_PID | LOG_CONS, 0);
  syslog(LOG_INFO, "%s started\n", argv[0]);

  if (gpioInitialise() < 0) {
    syslog(LOG_ERR, "pigpio initialization failed, program will quit.");
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
  
  pthread_t tids[3];
  
  if (
    pthread_create(&tids[0], NULL, thread_get_sensor_readings, &pl) != 0 ||
    pthread_create(&tids[1], NULL, thread_report_sensor_readings, &pl) != 0 ||
    pthread_create(&tids[2], NULL, thread_set_7seg_display, &pl) != 0
  ) {
    syslog(LOG_ERR, "Failed to create essentially threads, program will quit.");
    closelog();
    return 1;
  }

  for (int i = 0; i < sizeof(tids) / sizeof(tids[0]); ++i) {
    pthread_join(tids[i], NULL);
  }
  syslog(LOG_INFO, "Program quits gracefully.");
  closelog();
  return 0;
}
