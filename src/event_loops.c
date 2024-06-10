#include "global_vars.h"
#include "module.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <unistd.h>

void ev_collect_data() {
  syslog(LOG_INFO, "ev_collect_data() started");

  struct CollectionContext c_ctx = collection_init(gv_config_root);
  if (!c_ctx.init_success) {
    ev_flag = 1;
    SYSLOG_ERR("collection_init() initialization failed");
    goto err_collection_init;
  }

  struct PostCollectionContext pc_ctx;
  if (!(pc_ctx = post_collection_init(gv_config_root)).init_success) {
    SYSLOG_ERR(
        "post_collection_init() failed, post collection task will not run");
  }

  while (!ev_flag) {
    interruptible_sleep_us(gv_collection_event_interval_us);
    int ret;
    if ((ret = collection(&c_ctx)) < 0) {
      ev_flag = 1;
      SYSLOG_ERR("collection() encounters a fatal error (ret: %d)", ret);
      break;
    }
    if (ret > 0)
      syslog(LOG_WARNING,
             "collection() encounters a recoverable error (ret: %d), "
             "post_collection() call will be skipped",
             ret);
    if (!pc_ctx.init_success)
      continue;

    post_collection(&c_ctx, &pc_ctx);
  }

  post_collection_destory(&pc_ctx);
  collection_destory(&c_ctx);
err_collection_init:
  syslog(LOG_INFO, "ev_collect_data() exited gracefully.");
}
