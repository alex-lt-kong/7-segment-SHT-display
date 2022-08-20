#include <syslog.h>
int main() {
openlog("7std.out", LOG_PID | LOG_CONS, 0);
  syslog(LOG_INFO, "Start logging");
  closelog();
}
