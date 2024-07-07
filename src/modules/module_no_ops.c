#include "../module.h"

struct PostCollectionContext post_collection_init(const json_object *config) {

  struct PostCollectionContext ctx;
  ctx.init_success = true;
  return ctx;
}

int post_collection(struct CollectionContext *c_ctx,
                    struct PostCollectionContext *pc_ctx) {
  return 0;
}

void post_collection_destroy(struct PostCollectionContext *ctx) {
  // free(ctx->context);
}

struct CollectionContext collection_init(const json_object *config) {
  struct CollectionContext ctx = {.init_success = true, .context = NULL};
  return ctx;
}

int collection(void *ctx) { return 0; }

void void collection_destroy(struct CollectionContext *ctx) {
  // free(ctx->context);
}
