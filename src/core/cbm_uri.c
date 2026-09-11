#include "cbm_uri.h"
#include <string.h>
#include <stdio.h>

#define FNV1A_64_OFFSET 0xcbf29ce484222325ULL
#define FNV1A_64_PRIME  0x100000001b3ULL

uint64_t cbm_fnv1a_64(const void *data, size_t len) {
    if (!data) return 0;
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t hash = FNV1A_64_OFFSET;
    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= FNV1A_64_PRIME;
    }
    return hash;
}

int cbm_uri_parse(const char *raw_uri, CbmUri *out_uri) {
    if (!raw_uri || !out_uri) {
        return CBM_URI_ERR_NULL;
    }

    size_t raw_len = strlen(raw_uri);
    if (raw_len >= CBM_URI_MAX_LEN) {
        return CBM_URI_ERR_TOO_LONG;
    }

    const char *prefix = "cbm://";
    size_t prefix_len = strlen(prefix);
    if (strncmp(raw_uri, prefix, prefix_len) != 0) {
        return CBM_URI_MALFORMED_SYNTAX;
    }

    const char *after_prefix = raw_uri + prefix_len;
    const char *slash = strchr(after_prefix, '/');
    if (!slash || slash == after_prefix) {
        return CBM_URI_MALFORMED_SYNTAX;
    }

    size_t repo_len = (size_t)(slash - after_prefix);
    if (repo_len >= sizeof(out_uri->repo)) {
        return CBM_URI_ERR_TOO_LONG;
    }

    const char *path_start = slash + 1;
    const char *hash_pos = strchr(path_start, '#');
    if (!hash_pos || hash_pos == path_start) {
        /* Missing '#' fragment or empty path */
        return CBM_URI_MALFORMED_SYNTAX;
    }

    size_t path_len = (size_t)(hash_pos - path_start);
    if (path_len >= sizeof(out_uri->path)) {
        return CBM_URI_ERR_TOO_LONG;
    }

    const char *symbol_start = hash_pos + 1;
    size_t symbol_len = strlen(symbol_start);
    if (symbol_len == 0 || symbol_len >= sizeof(out_uri->symbol)) {
        /* Empty fragment or too long */
        return CBM_URI_MALFORMED_SYNTAX;
    }

    /* Populate fields */
    memset(out_uri, 0, sizeof(*out_uri));
    memcpy(out_uri->repo, after_prefix, repo_len);
    out_uri->repo[repo_len] = '\0';

    memcpy(out_uri->path, path_start, path_len);
    out_uri->path[path_len] = '\0';

    memcpy(out_uri->symbol, symbol_start, symbol_len);
    out_uri->symbol[symbol_len] = '\0';

    out_uri->hash = cbm_fnv1a_64(raw_uri, raw_len);

    return CBM_URI_OK;
}

int cbm_uri_to_string(const CbmUri *uri, char *buf, size_t buf_sz) {
    if (!uri || !buf || buf_sz == 0) {
        return -1;
    }
    int written = snprintf(buf, buf_sz, "cbm://%s/%s#%s", uri->repo, uri->path, uri->symbol);
    if (written < 0 || (size_t)written >= buf_sz) {
        return -1;
    }
    return 0;
}

bool cbm_uri_equals(const CbmUri *a, const CbmUri *b) {
    if (!a || !b) return false;
    if (a->hash != b->hash) return false;
    return (strcmp(a->repo, b->repo) == 0 &&
            strcmp(a->path, b->path) == 0 &&
            strcmp(a->symbol, b->symbol) == 0);
}
