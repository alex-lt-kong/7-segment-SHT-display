#include "utils.h"
#include "global_vars.h"

#include <json-c/json.h>

#include <limits.h>
#include <linux/limits.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

int load_values_from_json(const char *settings_path) {
  int retval = 0;
  json_object *root = json_object_from_file(settings_path);
  if (root == NULL) {
    SYSLOG_ERR("json_object_from_file(%s) returned NULL: %s", settings_path,
               json_util_get_last_err());
    retval = -1;
    goto err_json_parsing;
  }

  json_object *root_collection_event_interval_uc;
  json_object_object_get_ex(root, "collection_event_interval_us",
                            &root_collection_event_interval_uc);

  gv_collection_event_interval_us =
      json_object_get_uint64(root_collection_event_interval_uc);

  if (gv_collection_event_interval_us == 0) {
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

int interruptible_sleep_us(uint64_t us) {
  if (us < 1000 * 1000) {
    usleep(us);
    return 0;
  }
  uint32_t sec = us / (1000 * 1000);
  for (uint32_t i = 0; i < sec; ++i) {
    sleep(1);
    if (ev_flag) {
      return 1;
    }
  }
  return 0;
}
