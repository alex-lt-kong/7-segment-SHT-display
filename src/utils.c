#include "utils.h"
#include "global_vars.h"

#include <json-c/json.h>

#include <limits.h>
#include <linux/limits.h>
#include <string.h>
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

int load_values_from_json(const char *settings_path) {
  int retval = 0;
  json_object *root = json_object_from_file(settings_path);
  if (root == NULL) {
    SYSLOG_ERR("json_object_from_file(%s) returned NULL: %s", settings_path,
               json_util_get_last_err());
    retval = -1;
    goto err_json_parsing;
  }
  json_object *root_7sd;
  json_object_object_get_ex(root, "7seg_display", &root_7sd);
  json_object *root_7sd_display_digit_count;
  json_object_object_get_ex(root_7sd, "display_digit_count",
                            &root_7sd_display_digit_count);
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

  json_object *root_sht32_device_path;
  json_object_object_get_ex(root, "dht31_device_path", &root_sht32_device_path);
  json_object *root_cb_interval_sec;
  json_object_object_get_ex(root, "callback_interval_sec",
                            &root_cb_interval_sec);

  gv_display_digit_count = json_object_get_int(root_7sd_display_digit_count);
  gv_data_pin_num = json_object_get_int(root_7sd_data_pin_num);
  gv_clock_pin_num = json_object_get_int(root_7sd_clock_pin_num);
  gv_latch_pin_num = json_object_get_int(root_7sd_latch_pin_num);
  gv_chain_num = json_object_get_int(root_7sd_chain_num);

  strncpy(gv_gpiochip_path, json_object_get_string(root_7sd_gpiochip_path),
          PATH_MAX);
  strncpy(gv_dht31_device_path, json_object_get_string(root_sht32_device_path),
          PATH_MAX);

  gv_callback_interval_sec = json_object_get_uint64(root_cb_interval_sec);

  if (gv_display_digit_count == 0 || gv_data_pin_num == 0 ||
      gv_clock_pin_num == 0 || gv_latch_pin_num == 0 || gv_chain_num == 0 ||
      strlen(gv_gpiochip_path) == 0 || strlen(gv_dht31_device_path) == 0 ||
      gv_callback_interval_sec == 0) {
    SYSLOG_ERR("Some required values are not provided");
    retval = -2;
    goto err_invalid_config;
  }
  // Handle over the root to a global variable, it may be neeeded by callback
  // functions
  gv_config_root = root;
  return retval;

err_invalid_config:
  json_object_put(root);
  gv_config_root = NULL;
  root = NULL;
err_json_parsing:
  return retval;
}

int interruptible_sleep_sec(int sec) {
  for (int i = 0; i < sec; ++i) {
    sleep(1);
    if (ev_flag) {
      return 1;
    }
  }
  return 0;
}
