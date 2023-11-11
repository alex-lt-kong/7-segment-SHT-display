#include "global_vars.h"

#include <curl/curl.h>
#include <iotctrl/7segment-display.h>

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>

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
  const struct SensorPayload *pl = (struct SensorPayload *)payload;
  /* getenv()'s The caller must take care not tomodify this string,
     since that would change the environment of the process.*/
  const char *endpoint = getenv("SEVEN_SSD_TELEMETRY_ENDPOINT");
  const char *user = getenv("SEVEN_SSD_TELEMETRY_USER");
  const char *location = getenv("SEVEN_SSD_TELEMETRY_LOCATION");
  char json_data[1024];
  char timestamp_str[] = "1970-01-01T00:00:00Z";
  struct tm *utc_time;

  if (!endpoint || !user || !location) {
    (void)syslog(LOG_INFO, "The environment variables not found, "
                           "thread_report_sensor_readings() quits gracefully.");
    return NULL;
  }

  if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
    (void)syslog(LOG_ERR, "curl_global_init() failed, "
                          "thread_report_sensor_readings() quits gracefully.");
    goto err_curl_global_init;
  }

  struct curl_slist *headers = NULL;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  CURL *curl = curl_easy_init();
  if (!curl) {
    (void)syslog(LOG_ERR, "curl_easy_init() failed: %d(%s).", errno,
                 strerror(errno));
    goto err_curl_easy_init;
  }
  if (curl_easy_setopt(curl, CURLOPT_VERBOSE, 0) != CURLE_OK ||
      curl_easy_setopt(curl, CURLOPT_URL, endpoint) != CURLE_OK ||
      curl_easy_setopt(curl, CURLOPT_USERPWD, user) != CURLE_OK ||
      curl_easy_setopt(curl, CURLOPT_POST, 1L) != CURLE_OK ||
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers) != CURLE_OK) {
    (void)syslog(LOG_ERR, "curl_easy_setopt() failed.");
    goto err_curl_easy_setopt;
  }
  uint16_t iter = 3600 - 30;
  while (!done) {
    (void)sleep(1);
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
    (void)strftime(timestamp_str, sizeof(timestamp_str), "%Y-%m-%dT%H:%M:%SZ",
                   utc_time);

    if (pthread_mutex_lock(&my_mutex) != 0) {
      (void)syslog(LOG_ERR, "pthread_mutex_lock() failed: %d(%s).", errno,
                   strerror(errno));
      continue;
    }
    (void)snprintf(json_data, sizeof(json_data) / sizeof(json_data[0]),
                   "{\"temp\":%f,\"location\":\"%s\",\"timestamp_utc\":\"%s\"}",
                   pl->temp_celsius, location, timestamp_str);
    if (pthread_mutex_unlock(&my_mutex) != 0) {
      (void)syslog(LOG_ERR, "pthread_mutex_unlock() failed: %d(%s).", errno,
                   strerror(errno));
      done = 1;
    }

    if (curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                         (long)strlen(json_data)) != CURLE_OK) {
      (void)syslog(LOG_ERR, "curl_easy_setopt() failed.");
    }
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
      syslog(LOG_ERR, "curl_easy_perform() failed: %s",
             curl_easy_strerror(res));
    else {
      (void)syslog(LOG_INFO, "REST endpoint [%s] called", endpoint);
      long http_response_code;
      curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_response_code);
      if (http_response_code < 200 || http_response_code >= 300) {
        (void)syslog(LOG_ERR, "Unexpected HTTP status code: %ld.",
                     http_response_code);
      }
    }
  }
  syslog(LOG_INFO, "Stop signal received, "
                   "thread_report_sensor_readings() quits gracefully.");

err_curl_easy_setopt:
  (void)curl_slist_free_all(headers);
  (void)curl_easy_cleanup(curl);
err_curl_easy_init:
  (void)curl_global_cleanup();
err_curl_global_init:
  return NULL;
}

void *thread_get_sensor_readings(void *payload) {
  syslog(LOG_INFO, "thread_get_sensor_readings() started");
  // TODO: move to JSON config
  const struct iotctrl_7seg_display_connection conn = {.display_digit_count = 8,
                                                       .data_pin_num = 22,
                                                       .clock_pin_num = 11,
                                                       .latch_pin_num = 18,
                                                       .chain_num = 2};
  int fd;
  struct SensorPayload *pl = (struct SensorPayload *)payload;
  const char device_path[] = "/dev/i2c-1";

  if (iotctrl_init_display("/dev/gpiochip0", conn) != 0) {
    syslog(LOG_ERR, "iotctrl_init_display() failed, "
                    "thread_get_sensor_readings() won't start");
    return NULL;
  }

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
      iotctrl_update_value_two_four_digit_floats((float)pl->temp_celsius,
                                                 (float)pl->humidity);
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

  iotctrl_finalize_7seg_display();
  syslog(LOG_INFO, "Stop signal received, thread_get_sensor_readings() "
                   "quits gracefully.");
  return NULL;
}
