#define _GNU_SOURCE

#include "event_loops.h"
#include "global_vars.h"
#include "utils.h"

#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

static void signal_handler(int signum) {
  char msg[] = "Signal [  ] caught\n";
  msg[8] = '0' + (char)(signum / 10);
  msg[9] = '0' + (char)(signum % 10);
  write(STDIN_FILENO, msg, strlen(msg));
  done = 1;
}

int install_signal_handler() {
  // This design canNOT handle more than 99 signal types
  if (_NSIG > 99) {
    syslog(LOG_ERR, "signal_handler() can't handle more than 99 signals");
    return -1;
  }
  struct sigaction act;
  // Initialize the signal set to empty, similar to memset(0)
  if (sigemptyset(&act.sa_mask) == -1) {
    syslog(LOG_ERR, "sigemptyset(): %d(%s)", errno, strerror(errno));
    return -1;
  }
  act.sa_handler = signal_handler;
  /* SA_RESETHAND means we want our signal_handler() to intercept the
signal once. If a signal is sent twice, the default signal handler will be
used again. `man sigaction` describes more possible sa_flags. */
  act.sa_flags = SA_RESETHAND;
  // act.sa_flags = 0;
  if (sigaction(SIGINT, &act, 0) == -1 || sigaction(SIGABRT, &act, 0) == -1 ||
      sigaction(SIGTERM, &act, 0) == -1) {
    syslog(LOG_ERR, "sigaction(): %d(%s)", errno, strerror(errno));
    return -1;
  }
  return 0;
}
void print_usage(const char *binary_name) {

  printf("Usage: %s [OPTION]\n\n", binary_name);

  printf("Options:\n"
         "  --help,        -h        Display this help and exit\n"
         "  --config-path, -c        Path of JSON format configuration file\n");
}

const char *parse_args(int argc, char *argv[]) {
  static struct option long_options[] = {
      {"config-path", required_argument, 0, 'c'},
      {"help", optional_argument, 0, 'h'},
      {0, 0, 0, 0}};
  int opt, option_idx = 0;
  while ((opt = getopt_long(argc, argv, "c:h", long_options, &option_idx)) !=
         -1) {
    switch (opt) {
    case 'c':
      // optarg it is a pointer into the original argv array
      return optarg;
    }
  }
  print_usage(argv[0]);
  _exit(1);
}

int main(__attribute__((unused)) int argc, char **argv) {
  int retval = 0, r;

  const char *config_path = parse_args(argc, argv);
  load_values_from_json(config_path);
  openlog(PROGRAM_NAME, LOG_PID | LOG_CONS, LOG_USER);
  syslog(LOG_INFO, "%s started", PROGRAM_NAME);

  if ((r = install_signal_handler()) != 0) {
    retval = 1;
    syslog(LOG_ERR, "%s.%d: install_signal_handler() failed, retval: %d",
           __FILE__, __LINE__, r);
    goto err_sig_handler;
  }
  struct SensorPayload pl;
  pl.relative_humidity = 0;
  pl.temp_celsius = 0;
  pl.success = false;

  if (pthread_mutex_init(&my_mutex, NULL) != 0) {
    syslog(LOG_ERR, "pthread_mutex_init() failed: %d(%s), program will quit.",
           errno, strerror(errno));
    retval = 1;
    goto err_mutex_init;
  }

  pthread_t tids[2];
  if (pthread_create(&tids[0], NULL, thread_get_sensor_readings, &pl) != 0 ||
      pthread_create(&tids[1], NULL, thread_report_sensor_readings, &pl) != 0) {
    syslog(LOG_ERR, "pthread_create() failed: %d(%s), program will quit.",
           errno, strerror(errno));
    retval = 1;
    done = 1;
    goto err_pthread_create;
  }

  for (size_t i = 0; i < sizeof(tids) / sizeof(tids[0]); ++i) {
    if (pthread_join(tids[i], NULL) != 0) {
      syslog(LOG_ERR, "pthread_join() failed: %d(%s)", errno, strerror(errno));
      retval = 1;
    }
  }

  syslog(LOG_INFO, "Program quits gracefully.");
err_pthread_create:
  if (pthread_mutex_destroy(&my_mutex) != 0) {
    // But there is nothing else we can do on this.
    syslog(LOG_ERR, "pthread_mutex_destroy() failed: %d(%s)", errno,
           strerror(errno));
  }
err_mutex_init:
err_sig_handler:
  closelog();
  return retval;
}
