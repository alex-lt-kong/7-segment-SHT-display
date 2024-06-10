#include "global_vars.h"
#include "module.h"
#include "utils.h"

#include <iotctrl/7segment-display.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>

struct TempAndRHReadings {
  double temp_celsius;
  double relative_humidity;
  time_t update_time;
};

struct DHT31Handle {
  struct TempAndRHReadings readings;
  char *device_path;
};

struct PostCollectionContext post_collection_init(const json_object *config) {
  const json_object *root = config;
  json_object *root_7sd;
  json_object_object_get_ex(root, "7seg_display", &root_7sd);
  json_object *root_7sd_data_pin_num;
  json_object_object_get_ex(root_7sd, "data_pin_num", &root_7sd_data_pin_num);
  json_object *root_7sd_clock_pin_num;
  json_object_object_get_ex(root_7sd, "clock_pin_num", &root_7sd_clock_pin_num);
  json_object *root_7sd_latch_pin_num;
  json_object_object_get_ex(root_7sd, "latch_pin_num", &root_7sd_latch_pin_num);
  json_object *root_7sd_chain_num;
  json_object_object_get_ex(root_7sd, "chain_num", &root_7sd_chain_num);
  json_object *root_7sd_gpiochip_path;
  json_object_object_get_ex(root_7sd, "gpiochip_path", &root_7sd_gpiochip_path);

  struct iotctrl_7seg_disp_connection conn;
  conn.data_pin_num = json_object_get_int(root_7sd_data_pin_num);
  conn.clock_pin_num = json_object_get_int(root_7sd_clock_pin_num);
  conn.latch_pin_num = json_object_get_int(root_7sd_latch_pin_num);
  conn.chain_num = json_object_get_int(root_7sd_chain_num);
  conn.refresh_rate_hz = 500;
  strncpy(conn.gpiochip_path, json_object_get_string(root_7sd_gpiochip_path),
          PATH_MAX);
  if (conn.data_pin_num == 0 || conn.clock_pin_num == 0 ||
      conn.latch_pin_num == 0 || conn.chain_num == 0 ||
      strlen(conn.gpiochip_path) == 0) {
    SYSLOG_ERR("Some required values are not provided");
  }
  conn.refresh_rate_hz = 500;
  syslog(LOG_INFO, "data_pin_num: %d", conn.data_pin_num);
  syslog(LOG_INFO, "clock_pin_num: %d", conn.clock_pin_num);
  syslog(LOG_INFO, "latch_pin_num: %d", conn.latch_pin_num);
  syslog(LOG_INFO, "chain_num: %d", conn.chain_num);
  syslog(LOG_INFO, "gpiochip_path: %s", conn.gpiochip_path);
  struct PostCollectionContext ctx = {.init_success = true, .context = NULL};
  struct iotctrl_7seg_disp_handle *h;
  if ((h = iotctrl_7seg_disp_init(conn)) == NULL) {
    SYSLOG_ERR("iotctrl_7seg_disp_init() failed. Check stderr for "
               "possible internal error messages");
    ctx.init_success = false;
    ctx.context = NULL;
    return ctx;
  }
  ctx.context = h;
  return ctx;
}

int post_collection(struct CollectionContext *c_ctx,
                    struct PostCollectionContext *pc_ctx) {
  if (c_ctx == NULL)
    return -1;
  struct TempAndRHReadings r = ((struct DHT31Handle *)c_ctx->context)->readings;

  __auto_type led = (struct iotctrl_7seg_disp_handle *)pc_ctx->context;

  iotctrl_7seg_disp_update_as_four_digit_float(led, r.temp_celsius, 0);
  iotctrl_7seg_disp_update_as_four_digit_float(led, r.relative_humidity, 1);
  return 0;
}

void post_collection_destory(struct PostCollectionContext *ctx) {
  iotctrl_7seg_disp_destory((struct iotctrl_7seg_disp_handle *)(ctx->context));
}

struct CollectionContext collection_init(const json_object *config) {
  struct CollectionContext ctx = {.init_success = true, .context = NULL};
  struct DHT31Handle *h = malloc(sizeof(struct DHT31Handle));
  if (h == NULL) {
    SYSLOG_ERR("malloc() failed");
    goto err_malloc_handle;
  }

  const json_object *root = config;
  json_object *root_sht31_device_path;
  json_object_object_get_ex(root, "dht31_device_path", &root_sht31_device_path);
  const char *device_path = json_object_get_string(root_sht31_device_path);
  h->device_path = malloc(strlen(device_path) + 1);
  if (h->device_path == NULL)
    if (h == NULL) {
      SYSLOG_ERR("malloc() failed");
      goto err_malloc_device_path;
    }
  strcpy(h->device_path, device_path);

  h->readings.temp_celsius = 888.8;
  h->readings.relative_humidity = 888.8;
  h->readings.update_time = -1;
  ctx.context = h;

  return ctx;

err_malloc_device_path:
  free(h);
err_malloc_handle:
  ctx.init_success = false;
  return ctx;
}

/**
 * Performs a CRC8 calculation on the supplied values.
 *
 * @param data  Pointer to the data to use when calculating the CRC8.
 * @param len   The number of bytes in 'data'.
 *
 * @return The computed CRC8 value.
 */
uint8_t crc8(const uint8_t *data, int len) {
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

int collection(struct CollectionContext *ctx) {
  struct DHT31Handle *dht31 = (struct DHT31Handle *)ctx->context;
  int fd;
  if ((fd = open(dht31->device_path, O_RDWR)) < 0) {
    SYSLOG_ERR("Failed to open() device_path [%s], reading attempt will be "
               "skipped.",
               dht31->device_path);
    interruptible_sleep_us(5);
    goto err_dht31_io_error;
  }

  // Get I2C device, SHT31 I2C address is 0x44(68)
  if (ioctl(fd, I2C_SLAVE, 0x44) != 0) {
    SYSLOG_ERR("Failed to ioctl() device_path [%s]: %d(%s), reading "
               "attempt will be skipped.",
               dht31->device_path, errno, strerror(errno));
    interruptible_sleep_us(5);
    goto err_dht31_io_error;
  }

  // Send high repeatability measurement command
  // Command msb, command lsb(0x2C, 0x06)
  uint8_t config[2] = {0x2C, 0x06};
  if (write(fd, config, 2) != 2) {
    SYSLOG_ERR("Failed to write() command to [%s]: %d(%s), "
               "reading attempt will be skipped.",
               dht31->device_path, errno, strerror(errno));
    interruptible_sleep_us(5);
    goto err_dht31_io_error;
  }

  // Read 6 bytes of data
  // temp msb, temp lsb, temp CRC, humidity msb, humidity lsb,
  // humidity CRC
  uint8_t buf[6] = {0};

  if (read(fd, buf, 6) != 6) {
    SYSLOG_ERR("ailed to read() values from [%s]: %d(%s). This "
               "reading attempt will be skipped.",
               dht31->device_path, errno, strerror(errno));
    interruptible_sleep_us(5);
    goto err_dht31_io_error;
  }
  // Reference:
  // https://github.com/adafruit/Adafruit_SHT31/blob/bd465b980b838892964d2744d06ffc7e47b6fbef/Adafruit_SHT31.cpp#L197C8-L227
  float temp_celsius = (((buf[0] << 8) | buf[1]) * 175.0) / 65535.0 - 45.0;
  float relative_humidity = ((625 * ((buf[3] << 8) | buf[4])) >> 12) / 100.0;
  if (buf[2] != crc8(buf, 2) || buf[5] != crc8(buf + 3, 2)) {
    SYSLOG_ERR("Data read from [%s] but CRC8 failed. Retrieved (erroneous) "
               "readings are %f (temperature, °C), %f (relative humidity, %%)",
               dht31->device_path, temp_celsius, relative_humidity);
    goto err_crc_failed;
  }

  dht31->readings.temp_celsius = temp_celsius;
  dht31->readings.relative_humidity = relative_humidity;
  dht31->readings.update_time = time(NULL);
  if (dht31->readings.update_time == -1)
    SYSLOG_ERR("Failed to get time(): %d(%s)", errno, strerror(errno));

  return 0;

err_dht31_io_error:
err_crc_failed:
  close(fd);
  return 1;
}

void collection_destory(struct CollectionContext *ctx) {

  struct DHT31Handle *dht31 = (struct DHT31Handle *)ctx->context;
  free(dht31->device_path);
  dht31->device_path = NULL;
  free(dht31);
  dht31 = NULL;
}
