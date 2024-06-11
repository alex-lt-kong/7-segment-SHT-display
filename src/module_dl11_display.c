#include "global_vars.h"
#include "module.h"
#include "module_lib.h"
#include "utils.h"

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

struct PostCollectionContext post_collection_init(const json_object *config) {
  const json_object *root = config;
  json_object *root_7sd;
  json_object_object_get_ex(root, "7seg_display", &root_7sd);
  struct iotctrl_7seg_disp_handle *h = load_and_init_7seg(root_7sd);
  struct PostCollectionContext ctx;
  ctx.init_success = true;
  if (h == NULL) {
    ctx.init_success = false;
    return ctx;
  }
  ctx.context = h;
  return ctx;
}

int post_collection(struct CollectionContext *c_ctx,
                    struct PostCollectionContext *pc_ctx) {

  struct DL11MC *r = (struct DL11MC *)c_ctx->context;

  struct iotctrl_7seg_disp_handle *h =
      (struct iotctrl_7seg_disp_handle *)pc_ctx->context;
  iotctrl_7seg_disp_update_as_four_digit_float(h, r->temperature_celsius, 0);
  return 0;
}

void post_collection_destroy(struct PostCollectionContext *ctx) {
  iotctrl_7seg_disp_destroy((struct iotctrl_7seg_disp_handle *)(ctx->context));
}

struct CollectionContext collection_init(const json_object *config) {
  struct CollectionContext ctx = {.init_success = true, .context = NULL};
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
  ctx.context = d;

  return ctx;

err_malloc_device_path:
  free(d);
err_malloc_handle:
  ctx.init_success = false;
  return ctx;
}

int collection(struct CollectionContext *ctx) {
  struct DL11MC *dl11 = (struct DL11MC *)ctx->context;
  int res;
  const uint8_t sensor_count = 1;
  int16_t temps[sensor_count];
  int16_t temp;
  if ((res = iotctrl_get_temperature(dl11->device_path, sensor_count, temps,
                                     0)) != 0) {
    temp = 999;
    SYSLOG_ERR("iotctrl_get_temperature() failed, returned %d", res);
  } else {
    temp = temps[0];
  }

  dl11->temperature_celsius = temp / 10.0;
  syslog(LOG_INFO, "Readings changed to temp: %.1f°C",
         dl11->temperature_celsius);
  return 0;
}

void collection_destroy(struct CollectionContext *ctx) {
  struct DL11MC *dl11 = (struct DL11MC *)ctx->context;
  free(dl11->device_path);
  dl11->device_path = NULL;
  free(dl11);
  dl11 = NULL;
}
