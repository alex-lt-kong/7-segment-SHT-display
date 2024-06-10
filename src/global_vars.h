#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include "module.h"

#include <json-c/json.h>

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

// #define PROGRAM_NAME "temp-and-humidity-monitor"

extern json_object *gv_config_root;

extern volatile sig_atomic_t ev_flag;

extern uint64_t gv_collection_event_interval_us;

#endif // GLOBAL_VARS_H
