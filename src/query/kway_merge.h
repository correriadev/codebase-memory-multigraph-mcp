#ifndef CBM_KWAY_MERGE_H
#define CBM_KWAY_MERGE_H

#include "../core/cbm_uri.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(__has_include)
  #if __has_include(<sqlite3.h>)
    #include <sqlite3.h>
  #elif __has_include("sqlite3.h")
    #include "sqlite3.h"
  #endif
#else
  #include <sqlite3.h>
#endif

#define CBM_MAX_MERGE_STREAMS 32

typedef struct {
    char key[CBM_URI_MAX_LEN];
    char payload[4096];
    int stream_index;
    bool is_shadowed;
} MergeRecord;

typedef struct {
    sqlite3_stmt *stmt;
    bool active;
} StreamCursor;

typedef struct {
    MergeRecord items[CBM_MAX_MERGE_STREAMS];
    size_t count;
} MinHeap;

typedef struct {
    StreamCursor streams[CBM_MAX_MERGE_STREAMS];
    size_t stream_count;
    MinHeap heap;
    uint64_t skip;
    uint64_t limit;
    uint64_t skipped_count;
    uint64_t emitted_count;
    char last_emitted_key[CBM_URI_MAX_LEN];
} KWayMergeContext;

/* Initialize K-Way merge context with LIMIT and SKIP */
int cbm_kway_merge_init(KWayMergeContext *ctx, uint64_t skip, uint64_t limit);

/* Add a stream cursor to the merge iterator */
int cbm_kway_merge_add_cursor(KWayMergeContext *ctx, sqlite3_stmt *stmt);

/* Prime the heap from all registered cursors */
int cbm_kway_merge_prime(KWayMergeContext *ctx);

/* Step to the next emitted record in streaming order */
int cbm_kway_merge_step(KWayMergeContext *ctx, MergeRecord *out_record, bool *has_more);

/* Release cursors */
void cbm_kway_merge_close(KWayMergeContext *ctx);

#endif /* CBM_KWAY_MERGE_H */
