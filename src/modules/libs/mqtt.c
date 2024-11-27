#include "../../utils.h"

#include <mosquitto.h>
#include <sys/syslog.h>

void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level,
                       const char *str) {
  (void)mosq;
  (void)userdata;
  switch (level) {
  // case MOSQ_LOG_DEBUG:
  case MOSQ_LOG_INFO:
    syslog(LOG_INFO, "%s", str);
    break;
  case MOSQ_LOG_NOTICE:
    syslog(LOG_INFO, "%s", str);
    break;
  case MOSQ_LOG_WARNING:
    syslog(LOG_WARNING, "%s", str);
    break;
  case MOSQ_LOG_ERR:
    syslog(LOG_ERR, "%s", str);
    break;
  }
}

void mosq_on_connect(struct mosquitto *mosq, void *obj, int reason_code) {
  (void)mosq;
  (void)obj;
  syslog(LOG_INFO, "mosq_on_connect: %s",
         mosquitto_connack_string(reason_code));
}

void mosq_on_disconnect(struct mosquitto *mosq, void *obj, int reason_code) {
  (void)mosq;
  (void)obj;
  syslog(LOG_INFO, "mosq_on_disconnect(): %s",
         mosquitto_connack_string(reason_code));
}

void mosq_on_publish(struct mosquitto *mosq, void *obj, int msg_id) {
  (void)mosq;
  (void)obj;
  syslog(LOG_INFO,
         "mosq_on_publish(): Message (msg_id: %d) has been published.", msg_id);
}

struct mosquitto *initMosquitto(const char *host, const char *ca_file_path,
                                const char *username, const char *password) {
  int rc;
  struct mosquitto *mosq;
  /* Required before calling other mosquitto functions */
  if ((rc = mosquitto_lib_init()) != MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_lib_init() failed: %s", mosquitto_strerror(rc));
    goto err_init_mosquitto;
  }

  mosq = mosquitto_new(NULL, true, NULL);
  if (mosq == NULL) {
    SYSLOG_ERR("mosquitto_new() failed");
    goto err_init_mosquitto;
  }
  if ((rc = mosquitto_username_pw_set(mosq, username, password)) !=
      MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_username_pw_set() failed: %s",
               mosquitto_strerror(rc));
    goto err_mosquitto_config;
  }
  if ((rc = mosquitto_tls_set(mosq, ca_file_path, NULL, NULL, NULL, NULL)) !=
      MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_tls_set() failed: %s", mosquitto_strerror(rc));
    goto err_mosquitto_config;
  }
  mosquitto_connect_callback_set(mosq, mosq_on_connect);
  mosquitto_disconnect_callback_set(mosq, mosq_on_connect);
  mosquitto_publish_callback_set(mosq, mosq_on_publish);
  mosquitto_log_callback_set(mosq, mosq_log_callback);

  if ((rc = mosquitto_connect(mosq, host, 8883, 60)) != MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_connect() failed: %s", mosquitto_strerror(rc));
    goto err_mosquitto_connect;
  }

  /* Run the network loop in a background thread, this call returns quickly. */
  if ((rc = mosquitto_loop_start(mosq)) != MOSQ_ERR_SUCCESS) {
    SYSLOG_ERR("mosquitto_loop_start() failed: %s", mosquitto_strerror(rc));
    goto err_mosquitto_connect;
  }
  return mosq;

err_mosquitto_config:
err_mosquitto_connect:
  mosquitto_destroy(mosq);
err_init_mosquitto:
  return NULL;
}