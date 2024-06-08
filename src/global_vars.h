#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

#define PROGRAM_NAME "temp-and-humidity-monitor"

extern volatile sig_atomic_t ev_flag;
// No, we should not define my_mytex as volatile.
extern pthread_mutex_t my_mutex;

extern int gv_display_digit_count;
extern int gv_data_pin_num;
extern int gv_clock_pin_num;
extern int gv_latch_pin_num;
extern int gv_chain_num;

extern char gv_gpiochip_path[];
extern char gv_dht31_device_path[];

extern uint32_t gv_callback_interval_sec;

struct SensorPayload {
  double temp_celsius;
  double relative_humidity;
  bool success;
};

#endif // GLOBAL_VARS_H
