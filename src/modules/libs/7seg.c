#include "7seg.h"

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

struct iotctrl_7seg_disp_handle *
init_7seg_from_json(const json_object *config) {
  const json_object *root_7sd = config;
  json_object *root_7sd_data_pin_num;
  json_object_object_get_ex(root_7sd, "data_pin_num", &root_7sd_data_pin_num);
  json_object *root_7sd_clock_pin_num;
  json_object_object_get_ex(root_7sd, "clock_pin_num", &root_7sd_clock_pin_num);
  json_object *root_7sd_latch_pin_num;
  json_object_object_get_ex(root_7sd, "latch_pin_num", &root_7sd_latch_pin_num);
  json_object *root_7sd_chain_num;
  json_object_object_get_ex(root_7sd, "chain_num", &root_7sd_chain_num);
  json_object *root_7sd_refresh_rate;
  json_object_object_get_ex(root_7sd, "refresh_rate_hz",
                            &root_7sd_refresh_rate);
  json_object *root_7sd_gpiochip_path;
  json_object_object_get_ex(root_7sd, "gpiochip_path", &root_7sd_gpiochip_path);

  struct iotctrl_7seg_disp_connection conn;
  conn.data_pin_num = json_object_get_int(root_7sd_data_pin_num);
  conn.clock_pin_num = json_object_get_int(root_7sd_clock_pin_num);
  conn.latch_pin_num = json_object_get_int(root_7sd_latch_pin_num);
  conn.chain_num = json_object_get_int(root_7sd_chain_num);
  conn.refresh_rate_hz = json_object_get_int(root_7sd_refresh_rate);
  strncpy(conn.gpiochip_path, json_object_get_string(root_7sd_gpiochip_path),
          PATH_MAX);
  if (conn.data_pin_num == 0 || conn.clock_pin_num == 0 ||
      conn.latch_pin_num == 0 || conn.chain_num == 0 ||
      conn.refresh_rate_hz == 0 || strlen(conn.gpiochip_path) == 0) {
    fprintf(stderr, "Some required values are not provided");
    return NULL;
  }
  syslog(LOG_INFO, "7segment display parameters:");
  syslog(LOG_INFO, "data_pin_num: %d", conn.data_pin_num);
  syslog(LOG_INFO, "clock_pin_num: %d", conn.clock_pin_num);
  syslog(LOG_INFO, "latch_pin_num: %d", conn.latch_pin_num);
  syslog(LOG_INFO, "chain_num: %d", conn.chain_num);
  syslog(LOG_INFO, "refresh_rate_hz: %d", conn.refresh_rate_hz);
  syslog(LOG_INFO, "gpiochip_path: %s", conn.gpiochip_path);
  struct iotctrl_7seg_disp_handle *h;

  if ((h = iotctrl_7seg_disp_init(conn)) == NULL) {
    fprintf(stderr, "iotctrl_7seg_disp_init() failed. Check stderr for "
                    "possible internal error messages");
    return NULL;
  }
  return h;
}
