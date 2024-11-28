#ifndef MQTT_H
#define MQTT_H

#include <mosquitto.h>
#ifdef __cplusplus
extern "C" {
#endif

void mosq_log_callback(struct mosquitto *mosq, void *userdata, int level,
                       const char *str);

void mosq_on_connect(struct mosquitto *mosq, void *obj, int reason_code);

void mosq_on_disconnect(struct mosquitto *mosq, void *obj, int reason_code);

void mosq_on_publish(struct mosquitto *mosq, void *obj, int msg_id);

struct mosquitto *init_mosquitto(const char *host, const char *ca_file_path,
                                const char *username, const char *password);
                                
#ifdef __cplusplus
}
#endif

#endif // MQTT_H