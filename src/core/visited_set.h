#ifndef CBM_VISITED_SET_H
#define CBM_VISITED_SET_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define CBM_VISITED_CAP 8192
#define CBM_MAX_TRAVERSAL_DEPTH 5

typedef struct {
    uint64_t hashes[CBM_VISITED_CAP];
    size_t count;
    uint64_t *extra_hashes;
    size_t extra_count;
    size_t extra_cap;
    char **uris;
    char **extra_uris;
} VisitedSet;

static inline VisitedSet cbm_visited_init(void) {
    VisitedSet s;
    memset(&s, 0, sizeof(s));
    return s;
}

static inline void cbm_visited_free(VisitedSet *s) {
    if (!s) return;
    if (s->uris) {
        for (size_t i = 0; i < s->count; i++) {
            if (s->uris[i]) free(s->uris[i]);
        }
        free(s->uris);
        s->uris = NULL;
    }
    if (s->extra_uris) {
        for (size_t i = 0; i < s->extra_count; i++) {
            if (s->extra_uris[i]) free(s->extra_uris[i]);
        }
        free(s->extra_uris);
        s->extra_uris = NULL;
    }
    if (s->extra_hashes) {
        free(s->extra_hashes);
        s->extra_hashes = NULL;
    }
    s->count = 0;
    s->extra_count = 0;
    s->extra_cap = 0;
}

static inline bool cbm_visited_contains_uri(const VisitedSet *s, uint64_t hash, const char *uri) {
    if (!s) return false;
    for (size_t i = 0; i < s->count; i++) {
        if (s->hashes[i] == hash) {
            if (!uri) return true;
            if (s->uris && s->uris[i]) {
                if (strcmp(s->uris[i], uri) == 0) return true;
                /* Hash collision between distinct URIs: continue searching */
            } else {
                return true;
            }
        }
    }
    for (size_t i = 0; i < s->extra_count; i++) {
        if (s->extra_hashes && s->extra_hashes[i] == hash) {
            if (!uri) return true;
            if (s->extra_uris && s->extra_uris[i]) {
                if (strcmp(s->extra_uris[i], uri) == 0) return true;
                /* Hash collision between distinct URIs: continue searching */
            } else {
                return true;
            }
        }
    }
    return false;
}

static inline bool cbm_visited_contains(const VisitedSet *s, uint64_t hash) {
    return cbm_visited_contains_uri(s, hash, NULL);
}

static inline bool cbm_visited_add_uri(VisitedSet *s, uint64_t hash, const char *uri) {
    if (!s) return false;
    if (cbm_visited_contains_uri(s, hash, uri)) return true;

    if (s->count < CBM_VISITED_CAP) {
        if (uri) {
            if (!s->uris) {
                s->uris = (char **)calloc(CBM_VISITED_CAP, sizeof(char *));
                if (!s->uris) return false;
            }
            s->uris[s->count] = strdup(uri);
            if (!s->uris[s->count]) return false;
        }
        s->hashes[s->count++] = hash;
        return true;
    }

    /* Capacity beyond 8192: dynamically allocate and expand */
    if (s->extra_count >= s->extra_cap) {
        size_t new_cap = (s->extra_cap == 0) ? 1024 : s->extra_cap * 2;
        uint64_t *new_arr = (uint64_t *)realloc(s->extra_hashes, new_cap * sizeof(uint64_t));
        if (!new_arr) {
            return false; /* Allocation failure / resource exhaustion */
        }
        s->extra_hashes = new_arr;

        if (uri || s->extra_uris) {
            char **new_uris = (char **)realloc(s->extra_uris, new_cap * sizeof(char *));
            if (!new_uris) {
                return false;
            }
            for (size_t j = s->extra_cap; j < new_cap; j++) {
                new_uris[j] = NULL;
            }
            s->extra_uris = new_uris;
        }
        s->extra_cap = new_cap;
    }

    if (uri) {
        if (!s->extra_uris) {
            s->extra_uris = (char **)calloc(s->extra_cap, sizeof(char *));
            if (!s->extra_uris) return false;
        }
        s->extra_uris[s->extra_count] = strdup(uri);
        if (!s->extra_uris[s->extra_count]) return false;
    }
    s->extra_hashes[s->extra_count++] = hash;
    return true;
}

static inline bool cbm_visited_add(VisitedSet *s, uint64_t hash) {
    return cbm_visited_add_uri(s, hash, NULL);
}

#endif /* CBM_VISITED_SET_H */
