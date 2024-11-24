#include "../../utils.h"
#include "../module.h"

#include <iotctrl/dht31.h>
#include <iotctrl/temp-sensor.h>
#include <mosquitto.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

struct PostCollectionCtx {
  struct mosquitto *mosq;
  bool is_mqtt_connected;
  char *topic;
};

struct T2RHReadings {
  double temp_celsius0;
  double temp_celsius1;
  double relative_humidity;
};

struct ConnectionInfo {
  struct T2RHReadings readings;
  char *dht31_device_path;
  char *dl11_device_path;
};

void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level,
                       const char *str) {
  (void)mosq;
  switch (level) {
  // case MOSQ_LOG_DEBUG:
  // case MOSQ_LOG_INFO:
  // case MOSQ_LOG_NOTICE:
  case MOSQ_LOG_WARNING:
    printf("%i:%s\n", level, str);
  case MOSQ_LOG_ERR: {
    printf("%i:%s\n", level, str);
  }
  }
}

void on_connect(struct mosquitto *mosq, void *obj, int reason_code) {
  /* Print out the connection result. mosquitto_connack_string() produces an
   * appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
   * clients is mosquitto_reason_string().
   */
  struct PostCollectionCtx *ctx = (struct PostCollectionCtx *)obj;
  syslog(LOG_INFO, "mqtt on_connect()ed: %s",
         mosquitto_connack_string(reason_code));
  ctx->is_mqtt_connected = true;
}

void on_disconnect(struct mosquitto *mosq, void *obj, int reason_code) {
  (void)mosq;
  /* Print out the connection result. mosquitto_connack_string() produces an
   * appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
   * clients is mosquitto_reason_string().
   */
  struct PostCollectionCtx *ctx = (struct PostCollectionCtx *)obj;
  printf("mqtt on_disconnect()ed: %s\n", mosquitto_connack_string(reason_code));
  ctx->is_mqtt_connected = false;
}

/* Callback called when the client knows to the best of its abilities that a
 * PUBLISH has been successfully sent. For QoS 0 this means the message has
 * been completely written to the operating system. For QoS 1 this means we
 * have received a PUBACK from the broker. For QoS 2 this means we have
 * received a PUBCOMP from the broker. */
void on_publish(struct mosquitto *mosq, void *obj, int msg_id) {
  (void)mosq;
  (void)obj;
  syslog(LOG_INFO, "Message (msg_id: %d) has been published.", msg_id);
}

void *post_collection_init(const json_object *config) {
  struct PostCollectionCtx *ctx = malloc(sizeof(struct PostCollectionCtx));
  if (ctx == NULL)
    goto err_ctx_malloc;
  ctx->is_mqtt_connected = false;

  json_object *root;
  if (json_object_object_get_ex(config, "dd_mqtt", &root) == false) {
    SYSLOG_ERR("dd_mqtt not defined in config files");
    goto err_json_key_not_found;
  }
  json_object *root_ca_file_path;
  json_object *root_username;
  json_object *root_password;
  json_object *root_host;
  json_object *root_topic;
  const char *ca_file_path;
  const char *username;
  const char *password;
  const char *host;
  const char *topic;
  if (json_object_object_get_ex(root, "ca_file_path", &root_ca_file_path) ==
          false ||
      json_object_object_get_ex(root, "username", &root_username) == false ||
      json_object_object_get_ex(root, "password", &root_password) == false ||
      json_object_object_get_ex(root, "host", &root_host) == false ||
      json_object_object_get_ex(root, "topic", &root_topic) == false) {
    SYSLOG_ERR(
        "ca_file_path/username/password/host not defined in config files");
    goto err_json_key_not_found;
  }
  ca_file_path = json_object_get_string(root_ca_file_path);
  username = json_object_get_string(root_username);
  password = json_object_get_string(root_password);
  host = json_object_get_string(root_host);
  ctx->topic = (char *)json_object_get_string(root_topic);
  if (ca_file_path == NULL || username == NULL || password == NULL ||
      host == NULL || ctx->topic == NULL) {
    SYSLOG_ERR(
        "ca_file_path/username/password/host not defined in config files");
    goto err_json_key_not_found;
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
  ctx->mosq = mosquitto_new(NULL, true, NULL);
  if (ctx->mosq == NULL) {
    SYSLOG_ERR("mosquitto_new() failed");
    goto err_init_mosquitto;
  }
  if ((rc = mosquitto_username_pw_set(ctx->mosq, username, password)) !=
      MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_username_pw_set() failed: %s",
               mosquitto_strerror(rc));
    goto err_mosquitto_config;
  }
  if ((rc = mosquitto_tls_set(ctx->mosq, ca_file_path, NULL, NULL, NULL,
                              NULL)) != MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_tls_set() failed: %s", mosquitto_strerror(rc));
    goto err_mosquitto_config;
  }
  mosquitto_connect_callback_set(ctx->mosq, on_connect);
  mosquitto_disconnect_callback_set(ctx->mosq, on_connect);
  mosquitto_publish_callback_set(ctx->mosq, on_publish);
  mosquitto_log_callback_set(ctx->mosq, mosq_log_callback);

  /* Connect to test.mosquitto.org on port 1883, with a keepalive of 60
   * seconds. This call makes the socket connection only, it does not complete
   * the MQTT CONNECT/CONNACK flow, you should use mosquitto_loop_start() or
   * mosquitto_loop_forever() for processing net traffic. */
  rc = mosquitto_connect(ctx->mosq, "apps.mamsds.com", 8883, 60);
  if (rc != MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_connect() failed: %s", mosquitto_strerror(rc));
    goto err_mosquitto_connect;
  }

  /* Run the network loop in a background thread, this call returns quickly.
   */
  rc = mosquitto_loop_start(ctx->mosq);
  if (rc != MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_loop_start() failed: %s", mosquitto_strerror(rc));
    goto err_mosquitto_connect;
  }

  return ctx;
err_mosquitto_config:
err_mosquitto_connect:
  mosquitto_destroy(ctx->mosq);
err_init_mosquitto:
  free(ctx);
  ctx = NULL;
err_json_key_not_found:
err_ctx_malloc:
  return NULL;
}

int post_collection(void *c_ctx, void *pc_ctx) {
  struct T2RHReadings *_readings = (struct T2RHReadings *)c_ctx;
  struct PostCollectionCtx *_pc_ctx = (struct PostCollectionCtx *)pc_ctx;
  char payload[128];
  int rc;

  time_t now;
  struct tm *utc_time;
  char iso_time[21];
  time(&now);
  utc_time = gmtime(&now);
  strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", utc_time);

  snprintf(payload, sizeof(payload),
           "{\"timestamp\": \"%s\", \"temp0_celsius\": %.1f, "
           "\"temp1_celsius\": %.1f, "
           "\"relative_humidity\": %.1f}",
           iso_time, _readings->temp_celsius0, _readings->temp_celsius1,
           _readings->relative_humidity);

  /* Publish the message
   * mosq - our client instance
   * *mid = NULL - we don't want to know what the message id for this message
   * is topic = "example/temperature" - the topic on which this message will
   * be published payloadlen = strlen(payload) - the length of our payload in
   * bytes payload - the actual payload qos = 2 - publish with QoS 2 for this
   * example retain = false - do not use the retained message feature for this
   * message
   */
  rc = mosquitto_publish(_pc_ctx->mosq, NULL, _pc_ctx->topic, strlen(payload),
                         payload, 2, false);
  if (rc != MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("Error publishing: %s", mosquitto_strerror(rc));
    return 1;
  } else {
    syslog(LOG_INFO, "Publishing message [%s] to topic [%s]", payload,
           _pc_ctx->topic);
  }
  return 0;
}

void post_collection_destroy(void *ctx) {
  struct PostCollectionCtx *_ctx = (struct PostCollectionCtx *)ctx;
  mosquitto_lib_cleanup();
  free(_ctx);
}

void *collection_init(const json_object *config) {
  struct ConnectionInfo *conn = malloc(sizeof(struct ConnectionInfo));
  if (conn == NULL) {
    SYSLOG_ERR("malloc() failed");
    goto err_malloc_conn;
  }

  json_object *root;
  if (json_object_object_get_ex(config, "dd_mqtt", &root) == false) {
    SYSLOG_ERR("dd_mqtt not defined in config files");
    goto err_dd_mqtt_section_not_found;
  }

  const char *device_path;
  json_object *root_dht31_device_path;
  if (json_object_object_get_ex(root, "dht31_device_path",
                                &root_dht31_device_path) == false) {
    SYSLOG_ERR("dht31_device_path not defined in config files");
    goto err_dht31_path_not_found;
  }
  device_path = json_object_get_string(root_dht31_device_path);
  if (device_path != NULL)
    conn->dht31_device_path = malloc(strlen(device_path) + 1);
  else
    conn->dht31_device_path = NULL;
  if (device_path == NULL) {
    SYSLOG_ERR("dht31_device_path initialization failed");
    goto err_malloc_dht31_path;
  }
  strcpy(conn->dht31_device_path, device_path);

  json_object *root_dl11_device_path;
  json_object_object_get_ex(root, "dl11_device_path", &root_dl11_device_path);
  device_path = (char *)json_object_get_string(root_dl11_device_path);
  if (device_path != NULL)
    conn->dl11_device_path = malloc(strlen(device_path) + 1);
  else
    conn->dl11_device_path = NULL;
  if (conn->dl11_device_path == NULL) {
    SYSLOG_ERR("dl11_device_path initialization failed");
    goto err_malloc_dl11_path;
  }
  strcpy(conn->dl11_device_path, device_path);

  conn->readings.temp_celsius0 = 888.8;
  conn->readings.temp_celsius1 = 888.8;
  conn->readings.relative_humidity = 888.8;
  syslog(LOG_INFO,
         "collection_init() success, dht31_device_path: %s, "
         "dl11_device_path: %s",
         conn->dht31_device_path, conn->dl11_device_path);
  return conn;

err_malloc_dl11_path:
  free(conn->dht31_device_path);
err_dd_mqtt_section_not_found:
err_dht31_path_not_found:
err_malloc_dht31_path:
  free(conn);
err_malloc_conn:
  return NULL;
}

int collection(void *ctx) {
  struct ConnectionInfo *conn = (struct ConnectionInfo *)ctx;
  float temp_celsius_t;
  float relative_humidity_t;
  int ret = 0;
  const uint8_t sensor_count = 1;
  int16_t readings[sensor_count];
  int fd = iotctrl_dht31_init(conn->dht31_device_path);

  if ((ret = iotctrl_dht31_read(fd, &temp_celsius_t, &relative_humidity_t)) !=
      0) {
    syslog(LOG_INFO, "iotctrl_dht31_read() failed: %d", ret);
    ret = 1;
    goto err_dht31_read;
  }
  conn->readings.temp_celsius0 = temp_celsius_t;
  conn->readings.relative_humidity = relative_humidity_t;

  if (iotctrl_get_temperature(conn->dl11_device_path, sensor_count, readings,
                              0) != 0) {
    ret = 2;
    syslog(LOG_INFO, "iotctrl_get_temperature() failed: %d", ret);
    goto err_dl11_read;
  }
  conn->readings.temp_celsius1 = readings[0] / 10.0;

  syslog(LOG_INFO,
         "Readings changed to temp0: %.1f°C, temp1: %.1f°C, RH: %.1f%%",
         conn->readings.temp_celsius0, conn->readings.temp_celsius1,
         conn->readings.relative_humidity);
err_dl11_read:
err_dht31_read:
  iotctrl_dht31_destroy(fd);
  return ret;
}

void collection_destroy(void *ctx) {
  struct ConnectionInfo *conn = (struct ConnectionInfo *)ctx;
  free(conn->dht31_device_path);
  conn->dht31_device_path = NULL;
  free(conn->dl11_device_path);
  conn->dl11_device_path = NULL;
  free(conn);
  conn = NULL;
}
