#define _GNU_SOURCE
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/i2c-dev.h>
#include <netdb.h>
#include <pigpio.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>

#include "7seg.c"

volatile sig_atomic_t done = 0;
// No, we should not define my_mytex as volatile.
pthread_mutex_t my_mutex;

struct SensorPayload {
  double temp_celsius;
  double humidity;
  bool success;
};

/**
 * Performs a CRC8 calculation on the supplied values.
 *
 * @param data  Pointer to the data to use when calculating the CRC8.
 * @param len   The number of bytes in 'data'.
 *
 * @return The computed CRC8 value.
 */
static uint8_t crc8(const uint8_t *data, int len) {
  // Ref:
  // https://github.com/adafruit/Adafruit_SHT31/blob/bd465b980b838892964d2744d06ffc7e47b6fbef/Adafruit_SHT31.cpp#L163C4-L194

  const uint8_t POLYNOMIAL = 0x31;
  uint8_t crc = 0xFF;

  for (int j = len; j; --j) {
    crc ^= *data++;

    for (int i = 8; i; --i) {
      crc = (crc & 0x80) ? (crc << 1) ^ POLYNOMIAL : (crc << 1);
    }
  }
  return crc;
}

void *thread_report_sensor_readings(void *payload) {
  syslog(LOG_INFO, "thread_report_sensor_readings() started");
  struct SensorPayload *pl = (struct SensorPayload *)payload;
  /* getenv()'s The caller must take care not tomodify this string,
     since that would change the environment of the process.*/
  char *endpoint = getenv("SEVEN_SSD_TELEMETRY_ENDPOINT");
  char *user = getenv("SEVEN_SSD_TELEMETRY_USER");
  char *location = getenv("SEVEN_SSD_TELEMETRY_LOCATION");
  char json_data[1024];
  char timestamp_str[] = "1970-01-01T00:00:00Z";
  struct tm *utc_time;

  if (!endpoint || !user || !location) {
    syslog(LOG_INFO, "The environment variables not found, "
                     "thread_report_sensor_readings() quits gracefully.");
    return NULL;
  }
  uint16_t iter = 3000;

  curl_global_init(CURL_GLOBAL_DEFAULT);

  CURL *curl;
  CURLcode res;

  struct curl_slist *headers =
      curl_slist_append(NULL, "Content-Type: application/json");
  curl = curl_easy_init();
  if (!curl) {
    syslog(LOG_ERR, "curl_easy_init() failed: %d(%s).", errno, strerror(errno));
    goto err_curl_easy_init;
  }
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);
  curl_easy_setopt(curl, CURLOPT_URL, endpoint);
  curl_easy_setopt(curl, CURLOPT_USERPWD, user);
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  while (!done) {
    sleep(1);
    ++iter;
    if (iter < 3600) {
      continue;
    }
    iter = 0;

    time_t current_time = time(NULL);
    if (current_time == ((time_t)-1)) {
      syslog(LOG_ERR, "Faile to get time: %d(%s). This iteration is skipped",
             errno, strerror(errno));
      continue;
    }
    /* gmtime()'s return value points to a statically allocated struct which
       might be overwritten by subse‐ quent calls to any of the date and time
       functions.*/
    utc_time = gmtime(&current_time);
    if (utc_time == NULL) {
      syslog(LOG_ERR,
             "Faile to get utc_time: %d(%s). This iteration is skipped", errno,
             strerror(errno));
      continue;
    }
    strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ",
             utc_time);

    if (pthread_mutex_lock(&my_mutex) != 0) {
      syslog(LOG_ERR, "pthread_mutex_lock() failed: %d(%s).", errno,
             strerror(errno));
      continue;
    }
    snprintf(json_data, sizeof(json_data) / sizeof(json_data[0]),
             "{\"temp\":%f,\"location\":\"%s\",\"timestamp_utc\":\"%s\"}",
             pl->temp_celsius, location, timestamp_str);
    if (pthread_mutex_unlock(&my_mutex) != 0) {
      syslog(LOG_ERR, "pthread_mutex_unlock() failed: %d(%s).", errno,
             strerror(errno));
      done = 1;
    }

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(json_data));
    res = curl_easy_perform(curl);

    if (res != CURLE_OK)
      syslog(LOG_ERR, "curl_easy_perform() failed: %s",
             curl_easy_strerror(res));
    else {
      syslog(LOG_INFO, "REST endpoint [%s] called", endpoint);
      long http_response_code;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response_code);
      if (http_response_code < 200 || http_response_code >= 300) {
        syslog(LOG_ERR, "Unexpected HTTP status code: %ld.",
               http_response_code);
      }
    }
  }
  syslog(LOG_INFO, "Stop signal received, "
                   "thread_report_sensor_readings() quits gracefully.");

  curl_easy_cleanup(curl);
  curl_slist_free_all(headers);
err_curl_easy_init:
  curl_global_cleanup();
  return NULL;
}

void *thread_get_sensor_readings(void *payload) {
  syslog(LOG_INFO, "thread_get_sensor_readings() started");
  int fd;
  struct SensorPayload *pl = (struct SensorPayload *)payload;
  const char device_path[] = "/dev/i2c-1";
  while (!done) {
    for (int i = 0; i < 3; ++i) { // per some specs sheet online,
      // the frequency of DHT31 is 1hz.
      sleep(1);
      if (done) {
        break;
      }
    }
    if ((fd = open(device_path, O_RDWR)) < 0) {
      syslog(LOG_ERR,
             "Failed to open() device_path [%s], reading attempt will be "
             "skipped.",
             device_path);
      sleep(5);
      continue;
    }

    // Get I2C device, SHT31 I2C address is 0x44(68)
    if (ioctl(fd, I2C_SLAVE, 0x44) != 0) {
      syslog(LOG_ERR,
             "Failed to ioctl() device_path [%s]: %d(%s), reading "
             "attempt will be skipped.",
             device_path, errno, strerror(errno));
      sleep(5);
      continue;
    }

    // Send high repeatability measurement command
    // Command msb, command lsb(0x2C, 0x06)
    uint8_t config[2] = {0x2C, 0x06};
    if (write(fd, config, 2) != 2) {
      syslog(LOG_ERR,
             "Failed to write() command to [%s]: %d(%s), "
             "reading attempt will be skipped.",
             device_path, errno, strerror(errno));
      sleep(5);
      goto err_write_cmd;
    }

    // Read 6 bytes of data
    // temp msb, temp lsb, temp CRC, humidity msb, humidity lsb,
    // humidity CRC
    uint8_t buf[6] = {0};
    if (pthread_mutex_lock(&my_mutex) != 0) {
      syslog(LOG_ERR, "pthread_mutex_lock() failed: %d(%s).", errno,
             strerror(errno));
      goto err_mutex_lock;
    }
    if (read(fd, buf, 6) != 6) {
      syslog(LOG_ERR,
             "Failed to read() values from [%s]: %d(%s). This "
             "reading attempt will be skipped.",
             device_path, errno, strerror(errno));
      pl->success = false;
    } else if (buf[2] != crc8(buf, 2) || buf[5] != crc8(buf + 3, 2)) {
      syslog(LOG_ERR, "Data read from [%s] but CRC8 failed.", device_path);
      pl->success = false;
    } else {
      // Ref:
      // https://github.com/adafruit/Adafruit_SHT31/blob/bd465b980b838892964d2744d06ffc7e47b6fbef/Adafruit_SHT31.cpp#L197C8-L227
      pl->temp_celsius = (((buf[0] << 8) | buf[1]) * 175.0) / 65535.0 - 45.0;
      pl->humidity = ((625 * ((buf[3] << 8) | buf[4])) >> 12) / 100.0;
      pl->success = true;
    }
    if (pthread_mutex_unlock(&my_mutex) != 0) {
      syslog(LOG_ERR, "pthread_mutex_unlock() failed: %d(%s).", errno,
             strerror(errno));
      done = 1;
    }
  err_write_cmd:
  err_mutex_lock:
    close(fd);
  }
  syslog(LOG_INFO, "Stop signal received, thread_get_sensor_readings() "
                   "quits gracefully.");
  return NULL;
}

void *thread_set_7seg_display(void *payload) {
  syslog(LOG_INFO, "thread_set_7seg_display() started.");
  struct SensorPayload *pl = (struct SensorPayload *)payload;
  init_7seg_display();
  uint8_t vals[DIGIT_COUNT];
  bool dots[DIGIT_COUNT] = {0, 0, 1, 0, 0, 0, 1, 0};
  uint32_t interval = 0;
  while (!done) {
    ++interval;
    if (pthread_mutex_lock(&my_mutex) != 0) {
      syslog(LOG_ERR, "pthread_mutex_lock() failed: %d(%s).", errno,
             strerror(errno));
      continue;
    }
    if (interval > 16 && pl->success == true) {
      int temp_celsius = pl->temp_celsius * 10;
      int humidity = pl->humidity * 10;
      vals[0] = 10;
      vals[1] = temp_celsius % 1000 / 100;
      vals[2] = temp_celsius % 100 / 10;
      vals[3] = temp_celsius % 10;
      vals[4] = humidity % 10000 / 1000;
      if (vals[4] == 0) {
        vals[4] = 10;
      }
      vals[5] = humidity % 1000 / 100;
      vals[6] = humidity % 100 / 10;
      vals[7] = humidity % 10;
      interval = 0;
    }
    if (pthread_mutex_unlock(&my_mutex) != 0) {
      syslog(LOG_ERR, "pthread_mutex_unlock() failed: %d(%s).", errno,
             strerror(errno));
      done = 1;
    }
    show(vals, dots);
  }

  syslog(LOG_INFO, "thread_set_7seg_display() quits gracefully.");
  return NULL;
}

static void signal_handler(int signum) {
  char msg[] = "Signal [  ] caught\n";
  msg[8] = '0' + (char)(signum / 10);
  msg[9] = '0' + (char)(signum % 10);
  write(STDIN_FILENO, msg, strlen(msg));
  done = 1;
}

int install_signal_handler() {
  // This design canNOT handle more than 99 signal types
  if (_NSIG > 99) {
    syslog(LOG_ERR, "signal_handler() can't handle more than 99 signals");
    return -1;
  }
  struct sigaction act;
  // Initialize the signal set to empty, similar to memset(0)
  if (sigemptyset(&act.sa_mask) == -1) {
    syslog(LOG_ERR, "sigemptyset(): %d(%s)", errno, strerror(errno));
    return -1;
  }
  act.sa_handler = signal_handler;
  /* SA_RESETHAND means we want our signal_handler() to intercept the
signal once. If a signal is sent twice, the default signal handler will be
used again. `man sigaction` describes more possible sa_flags. */
  act.sa_flags = SA_RESETHAND;
  // act.sa_flags = 0;
  if (sigaction(SIGINT, &act, 0) == -1 || sigaction(SIGABRT, &act, 0) == -1 ||
      sigaction(SIGTERM, &act, 0) == -1) {
    syslog(LOG_ERR, "sigaction(): %d(%s)", errno, strerror(errno));
    return -1;
  }
  return 0;
}

int main(int argc, char **argv) {
  int retval = 0;
  openlog("7ssd.out", LOG_PID | LOG_CONS, 0);
  syslog(LOG_INFO, "%s started\n", argv[0]);

  if (gpioInitialise() < 0) {
    syslog(LOG_ERR, "pigpio initialization failed, program will quit.");
    retval = 1;
    goto err_gpio;
  }

  // signal handler must be installer after gpioInitialise()--perhaps
  // it installs its signal handler as well...
  if (install_signal_handler() != 0) {
    retval = 1;
    goto err_sig_handler;
  }
  init_7seg_display();
  struct SensorPayload pl;
  pl.humidity = 0;
  pl.temp_celsius = 0;
  pl.success = false;

  if (pthread_mutex_init(&my_mutex, NULL) != 0) {
    syslog(LOG_ERR,
           "pthread_mutex_init() failed: %d(%s), "
           "program will quit.",
           errno, strerror(errno));
    retval = 1;
    goto err_mutex_init;
  }

  pthread_t tids[3];
  if (pthread_create(&tids[0], NULL, thread_get_sensor_readings, &pl) != 0 ||
      pthread_create(&tids[1], NULL, thread_report_sensor_readings, &pl) != 0 ||
      pthread_create(&tids[2], NULL, thread_set_7seg_display, &pl) != 0) {
    syslog(LOG_ERR,
           "pthread_create() failed: %d(%s), "
           "program will quit.",
           errno, strerror(errno));
    retval = 1;
    done = 1;
    goto err_pthread_create;
  }

  for (size_t i = 0; i < sizeof(tids) / sizeof(tids[0]); ++i) {
    if (pthread_join(tids[i], NULL) != 0) {
      syslog(LOG_ERR, "pthread_join() failed: %d(%s)", errno, strerror(errno));
      retval = 1;
    }
  }

  syslog(LOG_INFO, "Program quits gracefully.");
err_pthread_create:
  if (pthread_mutex_destroy(&my_mutex) != 0) {
    // But there is nothing else we can do on this.
    syslog(LOG_ERR, "pthread_mutex_destroy() failed: %d(%s)", errno,
           strerror(errno));
  }
err_mutex_init:
err_sig_handler:
  gpioTerminate();
err_gpio:
  closelog();
  return retval;
}
