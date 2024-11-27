#include "../../global_vars.h"
#include "../../utils.h"
#include "../libs/7seg.h"
#include "../libs/mqtt.h"
#include "../module.h"

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
  struct mosquitto *mosq;
  const char *topic;
};

void *post_collection_init(const json_object *config) {
  const json_object *root = config;

  struct CHContext *chctx = malloc(sizeof(struct CHContext));
  if (chctx == NULL) {
    SYSLOG_ERR("malloc() failed");
    goto err_malloc_chctx;
  }
  struct json_object *json_ele;
  const char *host = NULL;
  const char *username = NULL;
  const char *password = NULL;
  const char *ca_file_path = NULL;
  json_pointer_get((json_object *)config, "/ch/mqtt/host", &json_ele);
  host = json_object_get_string(json_ele);
  json_pointer_get((json_object *)config, "/ch/mqtt/username", &json_ele);
  username = json_object_get_string(json_ele);
  json_pointer_get((json_object *)config, "/ch/mqtt/password", &json_ele);
  password = json_object_get_string(json_ele);
  json_pointer_get((json_object *)config, "/ch/mqtt/ca_file_path", &json_ele);
  ca_file_path = json_object_get_string(json_ele);
  json_pointer_get((json_object *)config, "/ch/mqtt/topic", &json_ele);
  chctx->topic = json_object_get_string(json_ele);
  if (host == NULL || ca_file_path == NULL || username == NULL ||
      password == NULL || chctx->topic == NULL) {
    SYSLOG_ERR("Invalid configs");
    goto err_invalid_settings;
  }

  json_pointer_get((json_object *)config, "/ch/7seg_display", &json_ele);
  chctx->h = init_7seg_from_json(json_ele);
  if (chctx->h == NULL) {
    goto err_init_7seg_from_json;
  }

  int rc;

  /* Required before calling other mosquitto functions */
  if (mosquitto_lib_init() != MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_lib_init() failed");
    goto err_init_mosquitto;
  }
  /* Create a new client instance.
   * id = NULL -> ask the broker to generate a client id for us
   * clean session = true -> the broker should remove old sessions when we
   * connect obj = NULL -> we aren't passing any of our private data for
   * callbacks
   */
  chctx->mosq = mosquitto_new(NULL, true, NULL);
  if (chctx->mosq == NULL) {
    SYSLOG_ERR("mosquitto_new() failed");
    goto err_init_mosquitto;
  }
  if ((rc = mosquitto_username_pw_set(chctx->mosq, username, password)) !=
      MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_username_pw_set() failed: %s",
               mosquitto_strerror(rc));
    goto err_mosquitto_config;
  }
  if ((rc = mosquitto_tls_set(chctx->mosq, ca_file_path, NULL, NULL, NULL,
                              NULL)) != MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_tls_set() failed: %s", mosquitto_strerror(rc));
    goto err_mosquitto_config;
  }
  mosquitto_connect_callback_set(chctx->mosq, mosq_on_connect);
  mosquitto_disconnect_callback_set(chctx->mosq, mosq_on_connect);
  mosquitto_publish_callback_set(chctx->mosq, mosq_on_publish);
  mosquitto_log_callback_set(chctx->mosq, mosq_log_callback);

  /*  This call makes the socket connection only, it does not complete
   * the MQTT CONNECT/CONNACK flow, you should use mosquitto_loop_start() or
   * mosquitto_loop_forever() for processing net traffic. */
  rc = mosquitto_connect(chctx->mosq, host, 8883, 60);
  if (rc != MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_connect() failed: %s", mosquitto_strerror(rc));
    goto err_mosquitto_connect;
  }

  /* Run the network loop in a background thread, this call returns quickly. */
  if ((mosquitto_loop_start(chctx->mosq)) != MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_loop_start() failed: %s", mosquitto_strerror(rc));
    goto err_mosquitto_connect;
  }

  return chctx;
err_mosquitto_config:
err_mosquitto_connect:
  mosquitto_destroy(chctx->mosq);
err_init_mosquitto:
err_init_7seg_from_json:
err_invalid_settings:
  free(chctx);
err_malloc_chctx:
  return NULL;
}

int post_collection(void *c_ctx, void *pc_ctx) {

  struct DL11MC *r = (struct DL11MC *)c_ctx;
  struct CHContext *chctx = (struct CHContext *)pc_ctx;
  struct iotctrl_7seg_disp_handle *h = chctx->h;
  iotctrl_7seg_disp_update_as_four_digit_float(h, r->temperature_celsius, 0);

  time_t now;
  struct tm *utc_time;
  char iso_time[21];
  time(&now);
  utc_time = gmtime(&now);
  strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", utc_time);

  char payload[128];
  snprintf(payload, sizeof(payload) - 1,
           "{\"timestamp\": \"%s\", \"temp_celsius\":%f}", iso_time,
           r->temperature_celsius);
  mosquitto_publish(chctx->mosq, NULL, chctx->topic, strlen(payload), payload,
                    1, false);
  return 0;
}

void post_collection_destroy(void *ctx) {
  if (ctx == NULL)
    return;
  struct CHContext *chctx = (struct CHContext *)ctx;
  iotctrl_7seg_disp_destroy(chctx->h);
  mosquitto_destroy(chctx->mosq);
  mosquitto_lib_cleanup();
  free(chctx);
}

void *collection_init(const json_object *config) {
  struct DL11MC *d = malloc(sizeof(struct DL11MC));
  if (d == NULL) {
    SYSLOG_ERR("malloc() failed");
    goto err_malloc_handle;
  }

  json_object *json_ele;
  json_pointer_get((json_object *)config, "/ch/dl11_device_path", &json_ele);
  const char *device_path = json_object_get_string(json_ele);
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
  if (ctx == NULL)
    return;
  struct DL11MC *dl11 = (struct DL11MC *)ctx;
  free(dl11->device_path);
  dl11->device_path = NULL;
  free(dl11);
  dl11 = NULL;
}
