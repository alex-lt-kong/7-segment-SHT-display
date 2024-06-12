#ifndef MODULE_LIB_H
#define MODULE_LIB_H

#include <json-c/json.h>

struct iotctrl_7seg_disp_handle *init_7seg_from_json(const json_object *config);

#endif // MODULE_LIB_H
