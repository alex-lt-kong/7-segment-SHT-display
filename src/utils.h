#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <syslog.h>

#define SYSLOG_ERR(format, ...)                                                \
  syslog(LOG_ERR, "[%s:%s()] " format, __FILE__, __func__, ##__VA_ARGS__)

int load_values_from_json(const char *settings_path);

/**
 * @brief This function only has microsecond level resolution if it sleeps for
 * less than 1 second
 * @return 0 if sleep returns after timeout, -1 if it is interrupted by signal
 */
int interruptible_sleep_us(uint64_t us);

#endif // UTILS_H
