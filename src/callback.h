// #include "utils.h"
#include "global_vars.h"

struct CallbackContext callback_init(const json_object *config);

int callback(const struct SensorReadings pl, struct CallbackContext *ctx);

void callback_destory(struct CallbackContext *ctx);
