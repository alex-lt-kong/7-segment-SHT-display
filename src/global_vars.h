#include <signal.h>
#include <stdbool.h>

extern volatile sig_atomic_t done;
// No, we should not define my_mytex as volatile.
extern pthread_mutex_t my_mutex;

struct SensorPayload {
  double temp_celsius;
  double humidity;
  bool success;
};