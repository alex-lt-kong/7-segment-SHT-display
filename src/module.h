#ifndef MODULE_H
#define MODULE_H

#include <json-c/json.h>

#include <stdbool.h>

struct CollectionContext {
  bool init_success;
  void *context;
};

struct PostCollectionContext {
  bool init_success;
  void *context;
};

struct PostCollectionContext post_collection_init(const json_object *config);

/**
 * @brief
 * @returns the return value is not used for the time being...
 */
int post_collection(struct CollectionContext *cl_ctx,
                    struct PostCollectionContext *cb_ctx);

void post_collection_destroy(struct PostCollectionContext *ctx);

struct CollectionContext collection_init(const json_object *config);

/**
 * @brief
 * @returns 0 on success; positive number on recoverable error (i.e., the event
 * loop can continue); negative number on fatal error (i.e., need to break the
 * data collection event loop)
 */
int collection(struct CollectionContext *ctx);

void collection_destroy(struct CollectionContext *ctx);

#endif // MODULE_H
