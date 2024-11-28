#ifndef MODULE_H
#define MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <json-c/json.h>

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
 * @note This function will only called if the init() function returns
 * a valid object (emulating C++ destructor's behavior). But implementers are
 * still advised to check for NULL if dereference is needed for resources release.
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
 * @note This function will only called if the init() function returns
 * a valid object (emulating C++ destructor's behavior). But implementers are
 * still advised to check for NULL if dereference is needed for resources release.
 */
void collection_destroy(void *ctx);

#ifdef __cplusplus
}
#endif

#endif // MODULE_H
