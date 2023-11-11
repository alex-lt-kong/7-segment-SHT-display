#include "global_vars.h"

volatile sig_atomic_t done = 0;
// No, we should not define my_mytex as volatile.
pthread_mutex_t my_mutex;
