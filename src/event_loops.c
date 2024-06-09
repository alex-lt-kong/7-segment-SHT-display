#include "callback.h"
#include "global_vars.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>

void *thread_callback() {
  syslog(LOG_INFO,
         "thread_callback() started, callback will be invoked every %d seconds",
         gv_callback_interval_sec);
  struct CallbackContext ctx;
  if (!(ctx = callback_init(gv_config_root)).init_success) {
    SYSLOG_ERR("callback_init() failed, callback event loop will quit");
    return NULL;
  }

  while (!ev_flag) {
    if (interruptible_sleep_sec(gv_callback_interval_sec) != 0)
      break;
    callback(gv_readings, &ctx);
  }

  callback_destory(&ctx);

  syslog(LOG_INFO, "thread_callback() exited gracefully");
  return NULL;
}

void *thread_get_sensor_readings() {
  syslog(LOG_INFO, "thread_get_sensor_readings() started");

  int fd;

  while (!ev_flag) {
    // per some specs sheet online, the frequency of DHT31 is 1hz.
    interruptible_sleep_sec(3);

    if ((fd = open(gv_dht31_device_path, O_RDWR)) < 0) {
      SYSLOG_ERR("Failed to open() device_path [%s], reading attempt will be "
                 "skipped.",
                 gv_dht31_device_path);
      interruptible_sleep_sec(5);
      continue;
    }

    // Get I2C device, SHT31 I2C address is 0x44(68)
    if (ioctl(fd, I2C_SLAVE, 0x44) != 0) {
      SYSLOG_ERR("Failed to ioctl() device_path [%s]: %d(%s), reading "
                 "attempt will be skipped.",
                 gv_dht31_device_path, errno, strerror(errno));
      interruptible_sleep_sec(5);
      goto err_dht31_io_error;
    }

    // Send high repeatability measurement command
    // Command msb, command lsb(0x2C, 0x06)
    uint8_t config[2] = {0x2C, 0x06};
    if (write(fd, config, 2) != 2) {
      SYSLOG_ERR("Failed to write() command to [%s]: %d(%s), "
                 "reading attempt will be skipped.",
                 gv_dht31_device_path, errno, strerror(errno));
      interruptible_sleep_sec(5);
      goto err_dht31_io_error;
    }

    // Read 6 bytes of data
    // temp msb, temp lsb, temp CRC, humidity msb, humidity lsb,
    // humidity CRC
    uint8_t buf[6] = {0};

    if (read(fd, buf, 6) != 6) {
      SYSLOG_ERR("ailed to read() values from [%s]: %d(%s). This "
                 "reading attempt will be skipped.",
                 gv_dht31_device_path, errno, strerror(errno));
      interruptible_sleep_sec(5);
      goto err_dht31_io_error;
    }
    // Reference:
    // https://github.com/adafruit/Adafruit_SHT31/blob/bd465b980b838892964d2744d06ffc7e47b6fbef/Adafruit_SHT31.cpp#L197C8-L227
    float temp_celsius = (((buf[0] << 8) | buf[1]) * 175.0) / 65535.0 - 45.0;
    float relative_humidity = ((625 * ((buf[3] << 8) | buf[4])) >> 12) / 100.0;
    if (buf[2] != crc8(buf, 2) || buf[5] != crc8(buf + 3, 2)) {
      SYSLOG_ERR(
          "Data read from [%s] but CRC8 failed. Retrieved (erroneous) "
          "readings are %f (temperature, celsius), %f (relative humidity, "
          "%%)",
          gv_dht31_device_path, temp_celsius, relative_humidity);
      goto err_crc_failed;
    }
    if (pthread_mutex_lock(&gv_sensor_readings_mtx) != 0) {
      SYSLOG_ERR("pthread_mutex_lock() failed: %d(%s).", errno,
                 strerror(errno));
      ev_flag = 1;
      break;
    }

    gv_readings.temp_celsius = temp_celsius;
    gv_readings.relative_humidity = relative_humidity;
    gv_readings.update_time = time(NULL);
    if (gv_readings.update_time == -1)
      SYSLOG_ERR("Failed to get time(): %d(%s)", errno, strerror(errno));

    if (pthread_mutex_unlock(&gv_sensor_readings_mtx) != 0) {
      SYSLOG_ERR("pthread_mutex_unlock() failed: %d(%s).", errno,
                 strerror(errno));
      ev_flag = 1;
      break;
    }

  err_crc_failed:
  err_dht31_io_error:
    close(fd);
  }

  syslog(LOG_INFO, "Stop signal received, thread_get_sensor_readings() "
                   "quits gracefully.");
  return NULL;
}
