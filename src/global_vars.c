#include "global_vars.h"

#include <limits.h>
#include <linux/limits.h>

volatile sig_atomic_t ev_flag = 0;

json_object *gv_config_root = NULL;

uint32_t gv_collection_event_interval_us = 60;
