#include "../module.h"

#include <stdbool.h>
#include <stdio.h>

struct PostCollectionCtx {
  uint32_t payload;
};
struct CollectionCtx {
  uint32_t payload;
};

void *post_collection_init(const json_object *config) {

  struct PostCollectionCtx *ctx = malloc(sizeof(struct PostCollectionCtx));
  if (ctx == NULL)
    return NULL;
  printf("post_collection_init() called\n");
  return ctx;
}

int post_collection(void *c_ctx, void *pc_ctx) {
  struct CollectionCtx *_c_ctx = (struct CollectionCtx *)c_ctx;
  struct PostCollectionCtx *_pc_ctx = (struct PostCollectionCtx *)pc_ctx;
  _pc_ctx->payload = _c_ctx->payload * 2;
  printf("post_collection() called, payload: %u\n", _pc_ctx->payload);
  return 0;
}

void post_collection_destroy(void *ctx) {
  struct PostCollectionCtx *_ctx = (struct PostCollectionCtx *)ctx;
  free(_ctx);
}

void *collection_init(const json_object *config) {
  struct CollectionCtx *ctx = malloc(sizeof(struct CollectionCtx));
  if (ctx == NULL)
    return NULL;
  ctx->payload = 0;
  printf("collection_init() called\n");
  return ctx;
}

int collection(void *ctx) {
  struct CollectionCtx *_ctx = (struct CollectionCtx *)ctx;
  ++(_ctx->payload);
  printf("collection() called\n");
  return 0;
}

void collection_destroy(void *ctx) {
  struct CollectionCtx *_ctx = (struct CollectionCtx *)ctx;
  free(_ctx);
}
