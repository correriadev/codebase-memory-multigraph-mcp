#ifndef CBM_URI_H
#define CBM_URI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CBM_URI_MAX_LEN 1024

#define CBM_URI_OK 0
#define CBM_URI_ERR_NULL -1
#define CBM_URI_MALFORMED_SYNTAX -2
#define CBM_URI_ERR_TOO_LONG -3

typedef struct {
    char repo[256];
    char path[512];
    char symbol[256];
    uint64_t hash;
} CbmUri;

/* Computes 64-bit FNV-1a hash of a byte buffer */
uint64_t cbm_fnv1a_64(const void *data, size_t len);

/* Parses a canonical cbm URI string (cbm://<repo>/<path>#<symbol>) into a CbmUri */
int cbm_uri_parse(const char *raw_uri, CbmUri *out_uri);

/* Formats a CbmUri into a canonical string buffer */
int cbm_uri_to_string(const CbmUri *uri, char *buf, size_t buf_sz);

/* Compares two CbmUri objects */
bool cbm_uri_equals(const CbmUri *a, const CbmUri *b);

#endif /* CBM_URI_H */
