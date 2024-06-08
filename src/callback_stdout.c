#include "callback.h"

#include <stdio.h>

int callback(struct SensorPayload pl) {
  printf("%f, %f\n", pl.temp_celsius, pl.relative_humidity);
  return 0;
}
