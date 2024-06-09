#include "callback.h"

#include <stdio.h>

struct CallbackContext callback_init(const json_object *config) {
  struct CallbackContext ctx = {.init_success = true, .context = NULL};
  return ctx;
}

int callback(const struct SensorReadings pl, struct CallbackContext *ctx) {
  printf("%f, %f\n", pl.temp_celsius, pl.relative_humidity);
  return 0;
}

void callback_destory(struct CallbackContext *ctx) {}
