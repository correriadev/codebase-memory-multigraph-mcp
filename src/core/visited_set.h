#ifndef CBM_VISITED_SET_H
#define CBM_VISITED_SET_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define CBM_VISITED_CAP 2048
#define CBM_MAX_TRAVERSAL_DEPTH 5

typedef struct {
    uint64_t hashes[CBM_VISITED_CAP];
    size_t count;
} VisitedSet;

static inline VisitedSet cbm_visited_init(void) {
    VisitedSet s;
    memset(&s, 0, sizeof(s));
    return s;
}

static inline bool cbm_visited_contains(const VisitedSet *s, uint64_t hash) {
    if (!s) return false;
    for (size_t i = 0; i < s->count; i++) {
        if (s->hashes[i] == hash) return true;
    }
    return false;
}

static inline bool cbm_visited_add(VisitedSet *s, uint64_t hash) {
    if (!s || s->count >= CBM_VISITED_CAP) return false;
    if (cbm_visited_contains(s, hash)) return false;
    s->hashes[s->count++] = hash;
    return true;
}

#endif /* CBM_VISITED_SET_H */
