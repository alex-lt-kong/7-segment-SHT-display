#include "../global_vars.h"
#include "../module.h"
#include "../module_lib.h"
#include "../utils.h"

#include <iotctrl/7segment-display.h>
#include <iotctrl/temp-sensor.h>

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

struct DL11MC {
  double temperature_celsius;
  char *device_path;
};

struct CHContext {
  struct iotctrl_7seg_disp_handle *h;
  time_t last_status_update_at;
  uint8_t (*month_days)[2];
  size_t date_count;
  char *external_program;
};

void *post_collection_init(const json_object *config) {
  const json_object *root = config;
  json_object *root_7sd;
  json_object_object_get_ex(root, "7seg_display", &root_7sd);

  struct CHContext *chctx = malloc(sizeof(struct CHContext));
  if (chctx == NULL) {
    SYSLOG_ERR("malloc() failed");
    goto err_malloc_chctx;
  }

  json_object *root_effective_dates;
  json_object_object_get_ex(root, "effective_dates", &root_effective_dates);
  chctx->date_count = json_object_array_length(root_effective_dates);
  if (chctx->date_count == 0) {
    SYSLOG_ERR("No effective_dates are defined");
    goto err_no_effective_dates;
  }

  chctx->last_status_update_at = 0;
  chctx->month_days = malloc(sizeof(uint8_t[chctx->date_count][2]));
  if (chctx->month_days == NULL) {
    SYSLOG_ERR("malloc() failed");
    goto err_malloc_month_days;
  }
  syslog(LOG_INFO, "Effective dates (in month, day) are:");
  for (size_t i = 0; i < chctx->date_count; ++i) {
    struct json_object *ele =
        json_object_array_get_idx(root_effective_dates, i);
    chctx->month_days[i][0] =
        json_object_get_int(json_object_array_get_idx(ele, 0));
    chctx->month_days[i][1] =
        json_object_get_int(json_object_array_get_idx(ele, 1));
    syslog(LOG_INFO, "%d, %d", chctx->month_days[i][0],
           chctx->month_days[i][1]);
  }

  json_object *root_external_command;
  json_object_object_get_ex(root, "external_command", &root_external_command);
  const char *external_prog = json_object_get_string(root_external_command);
  if (external_prog != NULL && strlen(external_prog) > 0) {
    chctx->external_program = malloc(strlen(external_prog) + 1);
    if (chctx->external_program == NULL) {
      SYSLOG_ERR("malloc() failed");
      goto err_external_program;
    }
    strcpy(chctx->external_program, external_prog);
    syslog(LOG_INFO, "external_program to execute on threshold crossing: [%s]",
           chctx->external_program);
  } else {
    syslog(LOG_INFO, "external_program is empty");
    goto err_external_program;
  }

  chctx->h = init_7seg_from_json(root_7sd);
  if (chctx->h == NULL) {
    goto err_init_7seg_from_json;
  }
  return chctx;

err_init_7seg_from_json:
  free(chctx->external_program);
err_external_program:
  free(chctx->month_days);
err_malloc_month_days:
err_no_effective_dates:
  free(chctx);
err_malloc_chctx:
  return NULL;
}

int post_collection(void *c_ctx, void *pc_ctx) {

  struct DL11MC *r = (struct DL11MC *)c_ctx;
  struct CHContext *chctx = (struct CHContext *)pc_ctx;
  struct iotctrl_7seg_disp_handle *h = chctx->h;
  iotctrl_7seg_disp_update_as_four_digit_float(h, r->temperature_celsius, 0);

  time_t t = time(NULL);
  int interval_sec = 7200;
  if (t - chctx->last_status_update_at < interval_sec)
    return 0;

  struct tm tm = *localtime(&t);
  bool is_effective = false;
  for (size_t i = 0; i < chctx->date_count; ++i) {
    if (chctx->month_days[i][0] == tm.tm_mon + 1 &&
        chctx->month_days[i][1] == tm.tm_mday) {
      is_effective = true;
      break;
    }
  }
  if (!is_effective) {
    syslog(LOG_INFO,
           "Today (%02d, %02d) is not an effective day, will retry later...",
           tm.tm_mon + 1, tm.tm_mday);
    chctx->last_status_update_at = t;
    return 0;
  }
  if (tm.tm_hour < 7 || tm.tm_hour > 21) {
    syslog(LOG_INFO,
           "Today (%02d, %02d) is an effective day but now (%dhrs) is out of "
           "effective hours, will retry later...",
           tm.tm_mon + 1, tm.tm_mday, tm.tm_hour);
    chctx->last_status_update_at = t;
    return 0;
  }
  float threshold = 28.0;
  /*syslog(LOG_INFO,
         "Today (%02d, %02d) is an effective day and %dhrs is within effective "
         "hours, checking temperature against threshold %f degrees Celsius",
         tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, threshold);*/
  if (r->temperature_celsius <= threshold)
    return 0;
  chctx->last_status_update_at = t;

  // By design, chctx->external_program should never be NULL
  if (chctx->external_program == NULL)
    return 0;
  char external_command[PATH_MAX];
  snprintf(external_command, PATH_MAX - 1, chctx->external_program,
           r->temperature_celsius);

  syslog(LOG_INFO,
         "Temperature reading (%f°C) is higer than the threshold, about to "
         "execute the external program: [%s]",
         r->temperature_celsius, external_command);

  int ret = 0;
  if ((ret = system(external_command)) != 0) {
    SYSLOG_ERR("The external program: returned non-zero value: %d", ret);
    return 1;
  }
  return 0;
}

void post_collection_destroy(void *ctx) {
  struct CHContext *chctx = (struct CHContext *)ctx;
  free(chctx->external_program);
  free(chctx->month_days);
  iotctrl_7seg_disp_destroy(chctx->h);
  free(chctx);
}

void *collection_init(const json_object *config) {
  struct DL11MC *d = malloc(sizeof(struct DL11MC));
  if (d == NULL) {
    SYSLOG_ERR("malloc() failed");
    goto err_malloc_handle;
  }

  const json_object *root = config;
  json_object *root_dl11_device_path;
  json_object_object_get_ex(root, "dl11_device_path", &root_dl11_device_path);
  const char *device_path = json_object_get_string(root_dl11_device_path);
  if (device_path != NULL)
    d->device_path = malloc(strlen(device_path) + 1);
  else
    d->device_path = NULL;
  if (d->device_path == NULL) {
    SYSLOG_ERR("d->device_path initialization failed");
    goto err_malloc_device_path;
  }
  strcpy(d->device_path, device_path);

  d->temperature_celsius = 888.8;
  return d;
err_malloc_device_path:
  free(d);
err_malloc_handle:
  return NULL;
}

int collection(void *ctx) {
  struct DL11MC *dl11 = (struct DL11MC *)ctx;
  int res;
  const uint8_t sensor_count = 1;
  int16_t temps[sensor_count];
  int16_t temp;
  if ((res = iotctrl_get_temperature(dl11->device_path, sensor_count, temps,
                                     0)) != 0) {
    temp = 999;
    SYSLOG_ERR("iotctrl_get_temperature() failed, returned %d", res);
    return 1;
  } else {
    temp = temps[0];
  }

  dl11->temperature_celsius = temp / 10.0;
  syslog(LOG_INFO, "Readings changed to temp: %.1f°C",
         dl11->temperature_celsius);
  return 0;
}

void collection_destroy(void *ctx) {
  struct DL11MC *dl11 = (struct DL11MC *)ctx;
  free(dl11->device_path);
  dl11->device_path = NULL;
  free(dl11);
  dl11 = NULL;
}
