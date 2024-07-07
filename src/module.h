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

/**
 * @brief Initialize a context object to be used by post_collection()
 * @return NULL on failure or a valid context object pointer
 */
void *post_collection_init(const json_object *config);

/**
 * @brief
 * @param ctx The CollectionContext pointer.
 * @param pc_ctx The PostCollectionContext pointer.
 * @returns the return value is not used for the time being...
 */
int post_collection(void *ctx, void *pc_ctx);

/**
 * @brief Release the resources allocated to/managed by the context object.
 */
void post_collection_destroy(void *pc_ctx);

/**
 * @brief Initialize a context object to be used by collection()
 * @return NULL on failure or a valid context object pointer
 */
void *collection_init(const json_object *config);

/**
 * @brief
 * @param ctx The context pointer initialized by collection_init().
 * @returns 0 on success; positive number on recoverable error (i.e., the event
 * loop can continue); negative number on fatal error (i.e., need to break the
 * data collection event loop)
 */
int collection(void *ctx);

/**
 * @brief Release the resources allocated to/managed by the context object.
 */
void collection_destroy(void *ctx);

#endif // MODULE_H
