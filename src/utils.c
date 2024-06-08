#include "global_vars.h"

#include <json-c/json.h>

#include <limits.h>
#include <linux/limits.h>
#include <string.h>
#include <syslog.h>

int load_values_from_json(const char *settings_path) {
  int retval = 0;
  json_object *root = json_object_from_file(settings_path);
  if (root == NULL) {
    syslog(LOG_ERR, "%s.%d: json_object_from_file(%s) returned NULL: %s",
           __FILE__, __LINE__, settings_path, json_util_get_last_err());
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

  gv_display_digit_count = json_object_get_int(root_7sd_display_digit_count);
  gv_data_pin_num = json_object_get_int(root_7sd_data_pin_num);
  gv_clock_pin_num = json_object_get_int(root_7sd_clock_pin_num);
  gv_latch_pin_num = json_object_get_int(root_7sd_latch_pin_num);
  gv_chain_num = json_object_get_int(root_7sd_chain_num);
  strncpy(gv_gpiochip_path, json_object_get_string(root_7sd_gpiochip_path),
          PATH_MAX);
  strncpy(gv_dht31_device_path, json_object_get_string(root_sht32_device_path),
          PATH_MAX);

  if (gv_display_digit_count == 0 || gv_data_pin_num == 0 ||
      gv_clock_pin_num == 0 || gv_latch_pin_num == 0 || gv_chain_num == 0 ||
      strlen(gv_gpiochip_path) == 0) {
    syslog(LOG_ERR, "%s.%d: Some required values are not provided", __FILE__,
           __LINE__);
    retval = -2;
    goto err_invalid_config;
  }
err_invalid_config:
  json_object_put(root);
err_json_parsing:
  return retval;
}
