#include "global_vars.h"

#include <limits.h>
#include <linux/limits.h>

volatile sig_atomic_t ev_flag = 0;
// No, we should not define my_mytex as volatile.
pthread_mutex_t gv_sensor_readings_mtx;

json_object *gv_config_root = NULL;
/*
int gv_display_digit_count = 0;
int gv_data_pin_num = 0;
int gv_clock_pin_num = 0;
int gv_latch_pin_num = 0;
int gv_chain_num = 0;

char gv_gpiochip_path[PATH_MAX + 1] = {0};*/
char gv_dht31_device_path[PATH_MAX + 1] = {0};

uint32_t gv_callback_interval_sec = 60;

struct SensorReadings gv_readings;
