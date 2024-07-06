#ifndef MODULE_H
#define MODULE_H

#include <json-c/json.h>

#include <stdbool.h>

struct CollectionContext {
  // The program will exit on init_success == false
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
 * @param cl_ctx The CollectionContext pointer. It is owned by the function
 * caller, i.e., the ev_collect_data() event loop
 * @param cb_ctx The PostCollectionContext pointer. It is owned by the function
 * caller, i.e., the ev_collect_data() event loop
 * @returns the return value is not used for the time being...
 */
int post_collection(struct CollectionContext *cl_ctx,
                    struct PostCollectionContext *cb_ctx);

/**
 * @brief
 * @param ctx The context pointer. It is owned by the function caller, i.e., the
 * ev_collect_data() event loop. You will need to release the resources
 * allocated to ctx->context, which is owned by this function.
 */
void post_collection_destroy(struct PostCollectionContext *ctx);

struct CollectionContext collection_init(const json_object *config);

/**
 * @brief
 * @param ctx The context pointer. It is owned by the function caller, i.e., the
 * ev_collect_data() event loop
 * @returns 0 on success; positive number on recoverable error (i.e., the event
 * loop can continue); negative number on fatal error (i.e., need to break the
 * data collection event loop)
 */
int collection(struct CollectionContext *ctx);

/**
 * @brief
 * @param ctx The context pointer. It is owned by the function caller, i.e., the
 * ev_collect_data() event loop. You will need to release the resources
 * allocated to ctx->context, which is owned by this function.
 */
void collection_destroy(struct CollectionContext *ctx);

#endif // MODULE_H
