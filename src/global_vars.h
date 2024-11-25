#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include "modules/module.h"

#include <json-c/json.h>

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>


extern json_object *gv_config_root;

extern volatile sig_atomic_t ev_flag;

extern uint64_t gv_collection_event_interval_us;

#endif // GLOBAL_VARS_H
