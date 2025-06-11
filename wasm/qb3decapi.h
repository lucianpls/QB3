#include <stdlib.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

extern "C" {
    // Take a look at a QB3 blob, return JSON info without full decoding
    // The json string is allocated by this function and must be freed by caller
    EMSCRIPTEN_KEEPALIVE
    char *GetInfo(void *data, size_t sz);

    // Full decode of a QB3 blob into a user-allocated buffer
    // Memory for outbuf and message(1024) must be allocated by caller
    // Size of outbuf must be what GetInfo returns in decoded_size field, in bytes
    // Message holds an error message if decoding fails, or is empty on success
    EMSCRIPTEN_KEEPALIVE
    size_t decode(void *data, size_t sz, void *outbuf, char *message);
}