#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <syslog.h>

#define SYSLOG_ERR(format, ...)                                                \
  syslog(LOG_ERR, "[%s:%s()] " format, __FILE__, __func__, ##__VA_ARGS__)

uint8_t crc8(const uint8_t *data, int len);

int load_values_from_json(const char *settings_path);

/**
 * @return 0 if sleep returns after timeout, -1 if it is interrupted by signal
 */
int interruptible_sleep_sec(int sec);

#endif // UTILS_H
