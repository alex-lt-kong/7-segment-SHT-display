#define FMT_HEADER_ONLY

#include "../module.h"
#include "../module_lib.h"

#include <cxxopts.hpp>
#include <fmt/core.h>
#include <iotctrl/7segment-display.h>
#include <iotctrl/dht31.h>
#include <iotctrl/temp-sensor.h>
#include <mosquitto.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <limits.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>

using namespace std;
using json = nlohmann::json;

struct Readings {
  double temp_outdoor_celsius;
  double temp_indoor_celsius;
  double rh_outdoor;
};

struct ConnectionInfo {
  struct Readings readings;
  char *dht31_device_path;
  char *dl11_device_path;
};

struct iotctrl_7seg_disp_handle *h0;
struct iotctrl_7seg_disp_handle *h1;
json settings;

/* Callback called when the client receives a CONNACK message from the broker.
 */
void mosquitto_on_connect(struct mosquitto *mosq, void *obj, int reason_code) {
  (void)obj;
  int rc;
  spdlog::info("mosquitto_on_connect(): {}",
               mosquitto_connack_string(reason_code));

  /* Making subscriptions in the mosquitto_on_connect() callback means that if
   * the connection drops and is automatically resumed by the client, then the
   * subscriptions will be recreated when the client reconnects. */
  rc = mosquitto_subscribe(
      mosq, NULL,
      settings.value("/dd_mqtt/topic"_json_pointer, "topic").c_str(), 1);
  if (rc != MOSQ_ERR_SUCCESS) {
    spdlog::error("mosquitto_subscribe() failed: {}", mosquitto_strerror(rc));
    /* We might as well disconnect if we were unable to subscribe */
    mosquitto_disconnect(mosq);
  }
}

/* Callback called when the broker sends a SUBACK in response to a SUBSCRIBE. */
void mosquitto_on_subscribe(struct mosquitto *mosq, void *obj, int mid,
                            int qos_count, const int *granted_qos) {
  (void)obj;
  (void)mid;
  int i;
  bool have_subscription = false;

  /* In this example we only subscribe to a single topic at once, but a
   * SUBSCRIBE can contain many topics at once, so this is one way to check
   * them all. */
  for (i = 0; i < qos_count; i++) {
    spdlog::info("mosquitto_on_subscribe: {}:granted qos = {}", i,
                 granted_qos[i]);
    if (granted_qos[i] <= 2) {
      have_subscription = true;
    }
  }
  if (have_subscription == false) {
    /* The broker rejected all of our subscriptions, we know we only sent
     * the one SUBSCRIBE, so there is no point remaining connected. */
    fprintf(stderr, "Error: All subscriptions rejected.\n");
    mosquitto_disconnect(mosq);
  }
}

/* Callback called when the client receives a message. */
void mosquitto_on_message(struct mosquitto *mosq, void *obj,
                          const struct mosquitto_message *msg) {
  (void)mosq;
  (void)obj;
  json payload;
  try {
    payload = json::parse((char *)msg->payload);
  } catch (const json::parse_error &e) {
    spdlog::error("Incoming message is invalid json: {}\n{}", e.what(),
                  (char *)msg->payload);
    return;
  }
  spdlog::info("{} {} {}", msg->topic, msg->qos, payload.dump());
  iotctrl_7seg_disp_update_as_four_digit_float(
      h0, payload.value("/temp_outdoor_celsius"_json_pointer, 888.8), 0);
  iotctrl_7seg_disp_update_as_four_digit_float(
      h0, payload.value("/rh_outdoor"_json_pointer, 888.8), 1);
  iotctrl_7seg_disp_update_as_four_digit_float(
      h1, payload.value("/temp_outdoor_celsius"_json_pointer, 888.8), 0);
  iotctrl_7seg_disp_update_as_four_digit_float(
      h1, payload.value("/temp_indoor_celsius"_json_pointer, 888.8), 1);
}

int main(int argc, char **argv) {
  int rc;
  struct mosquitto *mosq;
  cxxopts::Options options(argv[0], PROGRAM_NAME);
  string config_path;
  spdlog::set_pattern("%Y-%m-%d %T.%e | %7l | %5t | %v");
  // clang-format off
  options.add_options()
    ("h,help", "print help message")
    ("c,config-path", "JSON configuration file path", cxxopts::value<string>()->default_value(config_path));
  // clang-format on
  auto result = options.parse(argc, argv);
  if (result.count("help") || !result.count("config-path")) {
    std::cout << options.help() << "\n";
    return 0;
  }
  config_path = result["config-path"].as<std::string>();
  ifstream f(config_path);
  settings = json::parse(f);

  // clang-format off
    struct iotctrl_7seg_disp_connection conn0, conn1;
    conn0.data_pin_num = settings.value("/dd_mqtt/7seg_display0/data_pin_num"_json_pointer, 22);
    conn0.clock_pin_num = settings.value("/dd_mqtt/7seg_display0/clock_pin_num"_json_pointer, 11);
    conn0.latch_pin_num = settings.value("/dd_mqtt/7seg_display0/latch_pin_num"_json_pointer, 18);
    conn0.chain_num = settings.value("/dd_mqtt/7seg_display0/chain_num"_json_pointer, 2);
    conn0.refresh_rate_hz = settings.value("/dd_mqtt/7seg_display0/refresh_rate_hz"_json_pointer, 2000);
    strcpy(conn0.gpiochip_path,
    settings.value("/dd_mqtt/7seg_display0/gpiochip_path"_json_pointer, "/dev/gpiochip0").c_str());
  // clang-format on
  if ((h0 = iotctrl_7seg_disp_init(conn0)) == NULL) {
    spdlog::error("iotctrl_7seg_disp_init(conn0) failed. Check stderr for "
                  "possible internal error messages");
    goto err_h0_error;
  }
  // clang-format off
    conn1.data_pin_num = settings.value("/dd_mqtt/7seg_display1/data_pin_num"_json_pointer, 22);
    conn1.clock_pin_num = settings.value("/dd_mqtt/7seg_display1/clock_pin_num"_json_pointer, 11);
    conn1.latch_pin_num = settings.value("/dd_mqtt/7seg_display1/latch_pin_num"_json_pointer, 18);
    conn1.chain_num = settings.value("/dd_mqtt/7seg_display1/chain_num"_json_pointer, 2);
    conn1.refresh_rate_hz = settings.value("/dd_mqtt/7seg_display1/refresh_rate_hz"_json_pointer, 2000);
    strcpy(conn1.gpiochip_path, settings.value("/dd_mqtt/7seg_display1/gpiochip_path"_json_pointer,
    "/dev/gpiochip0").c_str());
  // clang-format on
  if ((h1 = iotctrl_7seg_disp_init(conn1)) == NULL) {
    spdlog::error("iotctrl_7seg_disp_init(conn1) failed. Check stderr for "
                  "possible internal error messages");

    goto err_h1_error;
  }

  mosquitto_lib_init();
  mosq = mosquitto_new(NULL, true, NULL);
  if (mosq == NULL) {
    spdlog::error("mosquitto_new() error:", mosquitto_strerror(rc));
    goto err_mosquitto_alloc;
  }
  if ((rc = mosquitto_username_pw_set(
           mosq,
           settings.value("/dd_mqtt/username"_json_pointer, "test").c_str(),
           settings.value("/dd_mqtt/password"_json_pointer, "test").c_str())) !=
      MOSQ_ERR_SUCCESS) {
    spdlog::error("mosquitto_username_pw_set() error: {}",
                  mosquitto_strerror(rc));
    goto err_mosquitto_init;
  }
  if ((rc = mosquitto_tls_set(
           mosq,
           settings.value("/dd_mqtt/ca_file_path"_json_pointer, "/tmp/ca.crt")
               .c_str(),
           NULL, NULL, NULL, NULL)) != MOSQ_ERR_SUCCESS) {
    spdlog::error("mosquitto_tls_set() error: {}", mosquitto_strerror(rc));
    goto err_mosquitto_init;
  }
  mosquitto_connect_callback_set(mosq, mosquitto_on_connect);
  mosquitto_subscribe_callback_set(mosq, mosquitto_on_subscribe);
  mosquitto_message_callback_set(mosq, mosquitto_on_message);

  rc = mosquitto_connect(
      mosq, settings.value("/dd_mqtt/host"_json_pointer, "localhost").c_str(),
      8883, 60);
  if (rc != MOSQ_ERR_SUCCESS) {
    spdlog::error("mosquitto_connect() error: {}", mosquitto_strerror(rc));
    goto err_mosquitto_init;
    return 1;
  }

  spdlog::info("mosquitto_loop_forever()...");
  mosquitto_loop_forever(mosq, -1, 1);

  mosquitto_destroy(mosq);
  mosquitto_lib_cleanup();
  return 0;
err_mosquitto_init:
  mosquitto_destroy(mosq);
err_mosquitto_alloc:
  iotctrl_7seg_disp_destroy(h1);
err_h1_error:
  iotctrl_7seg_disp_destroy(h0);
err_h0_error:
  return 0;
}
