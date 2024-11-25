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
  ev_flag = 1;
}

int install_signal_handler() {
  // This design canNOT handle more than 99 signal types
  if (_NSIG > 99) {
    SYSLOG_ERR("signal_handler() can't handle more than 99 signals");
    return -1;
  }
  struct sigaction act;
  // Initialize the signal set to empty, similar to memset(0)
  if (sigemptyset(&act.sa_mask) == -1) {
    SYSLOG_ERR("sigemptyset(): %d(%s)", errno, strerror(errno));
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
    SYSLOG_ERR("sigaction(): %d(%s)", errno, strerror(errno));
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

int main(int argc, char **argv) {
  int retval = 0, r;

  const char *config_path = parse_args(argc, argv);

  openlog(PROGRAM_NAME "_" MODULE_NAME, LOG_PID | LOG_CONS, LOG_USER);

  if ((r = load_values_from_json(config_path)) != 0) {
    retval = -1;
    SYSLOG_ERR("Failed to load settings from [%s]. retval: %d", config_path, r);
    goto err_config_file;
  }
  syslog(LOG_INFO, PROGRAM_NAME "_" MODULE_NAME " started");

  if ((r = install_signal_handler()) != 0) {
    retval = -2;
    SYSLOG_ERR("install_signal_handler() failed, retval: %d", r);
    goto err_sig_handler;
  }

  ev_collect_data();

  syslog(LOG_INFO, "Program quits gracefully.");

err_sig_handler:
  json_object_put(gv_config_root);
err_config_file:
  closelog();
  return retval;
}
