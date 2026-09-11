#include "kway_merge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int record_cmp(const MergeRecord *a, const MergeRecord *b) {
    int cmp = strcmp(a->key, b->key);
    if (cmp != 0) return cmp;
    /* If keys match, higher stream index (horizon overlay) takes precedence */
    return b->stream_index - a->stream_index;
}

static void min_heap_swap(MinHeap *h, size_t i, size_t j) {
    MergeRecord temp = h->items[i];
    h->items[i] = h->items[j];
    h->items[j] = temp;
}

static void min_heap_push(MinHeap *h, const MergeRecord *rec) {
    if (h->count >= CBM_MAX_MERGE_STREAMS) return;
    size_t i = h->count++;
    h->items[i] = *rec;
    while (i > 0) {
        size_t parent = (i - 1) / 2;
        if (record_cmp(&h->items[i], &h->items[parent]) < 0) {
            min_heap_swap(h, i, parent);
            i = parent;
        } else {
            break;
        }
    }
}

static bool min_heap_pop(MinHeap *h, MergeRecord *out_rec) {
    if (h->count == 0) return false;
    *out_rec = h->items[0];
    h->items[0] = h->items[--h->count];
    size_t i = 0;
    while (2 * i + 1 < h->count) {
        size_t left = 2 * i + 1;
        size_t right = 2 * i + 2;
        size_t smallest = left;
        if (right < h->count && record_cmp(&h->items[right], &h->items[left]) < 0) {
            smallest = right;
        }
        if (record_cmp(&h->items[smallest], &h->items[i]) < 0) {
            min_heap_swap(h, i, smallest);
            i = smallest;
        } else {
            break;
        }
    }
    return true;
}

int cbm_kway_merge_init(KWayMergeContext *ctx, uint64_t skip, uint64_t limit) {
    if (!ctx) return -1;
    memset(ctx, 0, sizeof(*ctx));
    ctx->skip = skip;
    ctx->limit = (limit == 0) ? UINT64_MAX : limit;
    ctx->last_emitted_key[0] = '\0';
    return 0;
}

int cbm_kway_merge_add_cursor(KWayMergeContext *ctx, sqlite3_stmt *stmt) {
    if (!ctx || !stmt || ctx->stream_count >= CBM_MAX_MERGE_STREAMS) return -1;
    size_t idx = ctx->stream_count++;
    ctx->streams[idx].stmt = stmt;
    ctx->streams[idx].active = true;
    return 0;
}

static bool advance_stream(KWayMergeContext *ctx, size_t stream_idx) {
    if (stream_idx >= ctx->stream_count || !ctx->streams[stream_idx].active) {
        return false;
    }
    sqlite3_stmt *stmt = ctx->streams[stream_idx].stmt;
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        MergeRecord rec;
        memset(&rec, 0, sizeof(rec));
        const char *k = (const char *)sqlite3_column_text(stmt, 0);
        const char *p = (const char *)sqlite3_column_text(stmt, 1);
        snprintf(rec.key, sizeof(rec.key), "%s", k ? k : "");
        snprintf(rec.payload, sizeof(rec.payload), "%s", p ? p : "");
        rec.stream_index = (int)stream_idx;
        rec.is_shadowed = false;
        min_heap_push(&ctx->heap, &rec);
        return true;
    } else {
        ctx->streams[stream_idx].active = false;
        return false;
    }
}

int cbm_kway_merge_prime(KWayMergeContext *ctx) {
    if (!ctx) return -1;
    for (size_t i = 0; i < ctx->stream_count; i++) {
        advance_stream(ctx, i);
    }
    return 0;
}

int cbm_kway_merge_step(KWayMergeContext *ctx, MergeRecord *out_record, bool *has_more) {
    if (!ctx || !out_record || !has_more) return -1;
    *has_more = false;
    memset(out_record, 0, sizeof(*out_record));
    out_record->key[0] = '\0';

    while (ctx->heap.count > 0 && (ctx->limit == 0 || ctx->limit == UINT64_MAX || ctx->emitted_count < ctx->limit)) {
        MergeRecord top;
        if (!min_heap_pop(&ctx->heap, &top)) {
            break;
        }

        /* Read next record from the popped stream into heap */
        advance_stream(ctx, (size_t)top.stream_index);

        /* Shadowing deduplication: if identical to last emitted key, shadow it */
        if (ctx->last_emitted_key[0] && strcmp(top.key, ctx->last_emitted_key) == 0) {
            continue;
        }

        /* Pagination: SKIP offset */
        if (ctx->skipped_count < ctx->skip) {
            ctx->skipped_count++;
            snprintf(ctx->last_emitted_key, sizeof(ctx->last_emitted_key), "%s", top.key);
            continue;
        }

        /* Emit row */
        *out_record = top;
        snprintf(ctx->last_emitted_key, sizeof(ctx->last_emitted_key), "%s", top.key);
        ctx->emitted_count++;
        *has_more = ((ctx->limit == 0 || ctx->limit == UINT64_MAX || ctx->emitted_count < ctx->limit) && ctx->heap.count > 0);
        return 0;
    }

    *has_more = false;
    out_record->key[0] = '\0';
    return 0;
}

void cbm_kway_merge_close(KWayMergeContext *ctx) {
    if (!ctx) return;
    for (size_t i = 0; i < ctx->stream_count; i++) {
        if (ctx->streams[i].stmt) {
            sqlite3_finalize(ctx->streams[i].stmt);
            ctx->streams[i].stmt = NULL;
        }
        ctx->streams[i].active = false;
    }
    ctx->stream_count = 0;
    ctx->heap.count = 0;
}
