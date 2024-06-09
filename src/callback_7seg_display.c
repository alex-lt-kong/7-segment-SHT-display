#include "callback.h"
#include "global_vars.h"
#include "utils.h"

#include <iotctrl/7segment-display.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

struct CallbackContext callback_init(const json_object *config) {
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
  struct CallbackContext ctx = {.init_success = true, .context = NULL};
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

int callback(const struct SensorReadings pl, struct CallbackContext *ctx) {
  struct iotctrl_7seg_disp_handle *h =
      (struct iotctrl_7seg_disp_handle *)ctx->context;
  iotctrl_7seg_disp_update_as_four_digit_float(h, (float)pl.temp_celsius, 0);
  iotctrl_7seg_disp_update_as_four_digit_float(h, (float)pl.relative_humidity,
                                               1);
  return 0;
}

void callback_destory(struct CallbackContext *ctx) {
  iotctrl_7seg_disp_destory((struct iotctrl_7seg_disp_handle *)(ctx->context));
}
