# Sensor data pipeline

Collect data from sensor and send the readings for further processing.

## Environment and dependency

- `cURL`: `apt install libcurl4-gnutls-dev`
- JSON: `apt install libjson-c-dev`

## Design

Essentially this project is a simple framework that does the following:

```C
// Initialize two context objects
void* ctx = collection_init();
void* pc_ctx = post_collection_init();

while (1) {
    // The data collected from sensors or whatever peripherals will be saved to ctx
    collection(ctx);
    // Then ctx and pc_ctx will be handed to post_collection(), it can display
    // the data on a 7seg digital tube or upload them to ElasticSearch or whatever.
    post_collection(ctx, pc_ctx);
}

post_collection_destroy(pc_ctx);
collection_destroy(ctx);
```
