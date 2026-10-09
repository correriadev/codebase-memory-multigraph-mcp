#include "recall_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char (*items)[CBM_URI_MAX_LEN];
    size_t count;
    size_t capacity;
} DynamicUriList;

static int dyn_list_add(DynamicUriList *list, const char *uri) {
    if (!list || !uri) return -1;
    if (list->count >= list->capacity) {
        size_t new_cap = (list->capacity == 0) ? 64 : list->capacity * 2;
        char (*new_items)[CBM_URI_MAX_LEN] = (char (*)[CBM_URI_MAX_LEN])realloc(list->items, new_cap * CBM_URI_MAX_LEN);
        if (!new_items) return -1;
        list->items = new_items;
        list->capacity = new_cap;
    }
    snprintf(list->items[list->count], CBM_URI_MAX_LEN, "%s", uri);
    list->count++;
    return 0;
}

static int cbm_bfs_reverse_deps_internal(sqlite3 *db, const CbmUri *contested_root, VisitedSet *visited, uint32_t max_depth, RecallReport *out_report, DynamicUriList *out_all_uris) {
    if (!db || !contested_root || !visited || !out_report) return -1;

    char root_str[CBM_URI_MAX_LEN];
    cbm_uri_to_string(contested_root, root_str, sizeof(root_str));

    size_t q_cap = 512;
    char (*queue)[CBM_URI_MAX_LEN] = (char (*)[CBM_URI_MAX_LEN])malloc(q_cap * CBM_URI_MAX_LEN);
    uint32_t *depth_queue = (uint32_t *)malloc(q_cap * sizeof(uint32_t));
    if (!queue || !depth_queue) {
        if (queue) free(queue);
        if (depth_queue) free(depth_queue);
        return -1;
    }

    size_t q_head = 0, q_tail = 0;

    snprintf(queue[q_tail], CBM_URI_MAX_LEN, "%s", root_str);
    depth_queue[q_tail] = 0;
    q_tail++;

    const char *causal_sql =
        "SELECT source_uri FROM virtual_edges WHERE target_uri = ? "
        "AND edge_type IN ('DEPENDS_ON', 'DERIVED_FROM', 'CALLS', 'IMPLEMENTS', 'EXTENDS');";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, causal_sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(queue);
        free(depth_queue);
        return -1;
    }

    while (q_head < q_tail) {
        char curr[CBM_URI_MAX_LEN];
        snprintf(curr, sizeof(curr), "%s", queue[q_head]);
        uint32_t depth = depth_queue[q_head];
        q_head++;

        uint64_t hash = cbm_fnv1a_64(curr, strlen(curr));
        if (cbm_visited_contains_uri(visited, hash, curr)) {
            continue;
        }
        if (!cbm_visited_add_uri(visited, hash, curr)) {
            sqlite3_finalize(stmt);
            free(queue);
            free(depth_queue);
            return -1;
        }

        if (strcmp(curr, root_str) != 0) {
            out_report->total_affected_count++;
            if (out_report->affected_count < CBM_RECALL_MAX_AFFECTED) {
                snprintf(out_report->affected_uris[out_report->affected_count], CBM_URI_MAX_LEN, "%s", curr);
                out_report->affected_count++;
            }
            if (out_all_uris) {
                if (dyn_list_add(out_all_uris, curr) != 0) {
                    sqlite3_finalize(stmt);
                    free(queue);
                    free(depth_queue);
                    return -1;
                }
            }
        }

        if (max_depth > 0 && depth >= max_depth) {
            continue;
        }

        /* Query causal derivation edges */
        sqlite3_bind_text(stmt, 1, curr, -1, SQLITE_STATIC);
        int step_rc = SQLITE_DONE;
        while ((step_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            const char *src = (const char *)sqlite3_column_text(stmt, 0);
            if (src && src[0]) {
                uint64_t nhash = cbm_fnv1a_64(src, strlen(src));
                if (!cbm_visited_contains_uri(visited, nhash, src)) {
                    if (q_tail >= q_cap) {
                        size_t new_cap = q_cap * 2;
                        char (*new_q)[CBM_URI_MAX_LEN] = (char (*)[CBM_URI_MAX_LEN])realloc(queue, new_cap * CBM_URI_MAX_LEN);
                        uint32_t *new_dq = (uint32_t *)realloc(depth_queue, new_cap * sizeof(uint32_t));
                        if (!new_q || !new_dq) {
                            if (new_q) queue = new_q;
                            if (new_dq) depth_queue = new_dq;
                            sqlite3_finalize(stmt);
                            free(queue);
                            free(depth_queue);
                            return -1;
                        }
                        queue = new_q;
                        depth_queue = new_dq;
                        q_cap = new_cap;
                    }
                    snprintf(queue[q_tail], CBM_URI_MAX_LEN, "%s", src);
                    depth_queue[q_tail] = depth + 1;
                    q_tail++;
                }
            }
        }
        if (step_rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            free(queue);
            free(depth_queue);
            return -1;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    free(queue);
    free(depth_queue);
    return 0;
}

int cbm_bfs_reverse_deps_acyclic(sqlite3 *db, const CbmUri *contested_root, VisitedSet *visited, uint32_t max_depth, RecallReport *out_report) {
    if (!db || !contested_root || !visited || !out_report) return -1;
    return cbm_bfs_reverse_deps_internal(db, contested_root, visited, max_depth, out_report, NULL);
}

int cbm_trigger_recall(sqlite3 *db, const CbmUri *contested_root, const char *reason, RecallReport *out_report) {
    if (out_report) {
        out_report->is_committed = false;
    }
    if (!db || !contested_root || !out_report) return -1;

    memset(out_report, 0, sizeof(*out_report));
    out_report->contested_root = *contested_root;
    out_report->is_committed = false;
    if (reason) {
        snprintf(out_report->reason, sizeof(out_report->reason), "%s", reason);
    }

    if (sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL) != SQLITE_OK) {
        out_report->is_committed = false;
        return CBM_RECALL_ERR_TX_FAILED;
    }

    DynamicUriList all_nodes = {0};
    VisitedSet visited = cbm_visited_init();
    /* max_depth == 0 means unbounded traversal */
    int rc = cbm_bfs_reverse_deps_internal(db, contested_root, &visited, 0, out_report, &all_nodes);
    cbm_visited_free(&visited);
    if (rc != 0) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        out_report->is_committed = false;
        if (all_nodes.items) free(all_nodes.items);
        return CBM_RECALL_ERR_TX_FAILED;
    }

    /* Mark contested_root and all affected nodes as CONTESTED */
    char root_str[CBM_URI_MAX_LEN];
    cbm_uri_to_string(contested_root, root_str, sizeof(root_str));

    sqlite3_stmt *stmt = NULL;
    const char *sql = "UPDATE symbolic_nodes SET epistemic_status = 'CONTESTED' WHERE cbm_uri = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        out_report->is_committed = false;
        if (all_nodes.items) free(all_nodes.items);
        return CBM_RECALL_ERR_TX_FAILED;
    }

    sqlite3_bind_text(stmt, 1, root_str, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        out_report->is_committed = false;
        if (all_nodes.items) free(all_nodes.items);
        return CBM_RECALL_ERR_TX_FAILED;
    }
    sqlite3_reset(stmt);

    for (size_t i = 0; i < all_nodes.count; i++) {
        sqlite3_bind_text(stmt, 1, all_nodes.items[i], -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            out_report->is_committed = false;
            if (all_nodes.items) free(all_nodes.items);
            return CBM_RECALL_ERR_TX_FAILED;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    if (all_nodes.items) {
        free(all_nodes.items);
    }

    if (sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        out_report->is_committed = false;
        return CBM_RECALL_ERR_TX_FAILED;
    }

    out_report->is_committed = true;
    return CBM_RECALL_OK;
}
