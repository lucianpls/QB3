#include "qb3decapi.h"
#include "json.hpp"
#include "../QB3lib/QB3.h"

static const char *type(qb3_dtype type) {
    switch (type) {
        case QB3_U8: return "uint8";
        case QB3_I8: return "int8";
        case QB3_U16: return "uint16";
        case QB3_I16: return "int16";
        case QB3_U32: return "uint32";
        case QB3_I32: return "int32";
        case QB3_U64: return "uint64";
        case QB3_I64: return "int64";
        default: return "unknown";
    }
}

static const char *mode(qb3_mode mode) {
    switch (mode) {
        // Legacy modes
        case QB3M_BASE_Z: return "base_z";
        case QB3M_CF: return "best_z";
        case QB3M_RLE: return "base_z_rle";
        case QB3M_CF_RLE: return "best_z_rle";

        // Standard modes
        case QB3M_BASE_H: return "base";
        case QB3M_BEST: return "best";
        case QB3M_RLE_H: return "base_rle";
        case QB3M_CF_H: return "best_rle";

#if defined(QB3_HAS_FTL)
        // Fastest mode
        case QB3M_FTL: return "ftl";
#endif

        case QB3M_STORED: return "stored";
        default: return "invalid";
    }
}

char *GetInfo(void *data, size_t sz) {
    nlohmann::json j;
    size_t image_size[3] = {0, 0, 0};
    decsp p = qb3_read_start(data, sz, image_size);
    
    if (!p) {
        j["error"] = "Invalid QB3 format";
        return strdup(j.dump().c_str());
    }

    if (!qb3_read_info(p)) {
        qb3_destroy_decoder(p);
        j["error"] = "Failed to read QB3 info";
        return strdup(j.dump().c_str());
    }

    j["xsize"] = image_size[0];
    j["ysize"] = image_size[1];
    j["nbands"] = image_size[2];
    j["dtype"] = type(qb3_get_type(p));
    j["mode"] = mode(qb3_get_mode(p));
    if (qb3_get_quanta(p) > 1)
        j["quanta"] = qb3_get_quanta(p);

    size_t cband[QB3_MAXBANDS] = {0};
    if (qb3_get_coreband(p, cband)) {
        j["bandmap"] = std::vector<size_t>(cband, cband + image_size[2]);
    } else {
        j["bandmap"] = nullptr;
    }

    qb3_destroy_decoder(p);
    return strdup(j.dump().c_str());
}

// Full decode
size_t decode(void *data, size_t sz, void *outbuf, char *message) {
    message[0] = '\0'; // Clear message buffer
    size_t image_size[3] = {0, 0, 0};
    decsp p = qb3_read_start(data, sz, image_size);
    if (!p) {
        strncpy(message, "Invalid QB3 format", 1024);
        return 0; // Indicate failure
    }
    if (!qb3_read_info(p)) {
        qb3_destroy_decoder(p);
        strncpy(message, "Failed to read QB3 info", 1024);
        return 0; // Indicate failure
    }

    auto read_bytes = qb3_read_data(p, outbuf);
    if (read_bytes == 0) {
        qb3_destroy_decoder(p);
        strncpy(message, "Failed to read QB3 data", 1024);
        return 0; // Indicate failure
    }
    qb3_destroy_decoder(p);
    return read_bytes; // Return number of bytes read
}


