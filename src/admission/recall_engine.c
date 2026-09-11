#include "recall_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cbm_bfs_reverse_deps_acyclic(sqlite3 *db, const CbmUri *contested_root, VisitedSet *visited, uint32_t max_depth, RecallReport *out_report) {
    if (!contested_root || !visited || !out_report) return -1;

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

    while (q_head < q_tail) {
        char curr[CBM_URI_MAX_LEN];
        snprintf(curr, sizeof(curr), "%s", queue[q_head]);
        uint32_t depth = depth_queue[q_head];
        q_head++;

        uint64_t hash = cbm_fnv1a_64(curr, strlen(curr));
        if (cbm_visited_contains(visited, hash)) {
            continue;
        }
        cbm_visited_add(visited, hash);

        if (strcmp(curr, root_str) != 0 && out_report->affected_count < CBM_RECALL_MAX_AFFECTED) {
            snprintf(out_report->affected_uris[out_report->affected_count], CBM_URI_MAX_LEN, "%s", curr);
            out_report->affected_count++;
        }

        if (depth >= max_depth || !db) {
            continue;
        }

        /* Query inbound callers: nodes that depend on curr (source_uri in virtual_edges where target_uri = curr) */
        const char *sql = "SELECT source_uri FROM virtual_edges WHERE target_uri = ?";
        sqlite3_stmt *stmt = NULL;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, curr, -1, SQLITE_STATIC);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *src = (const char *)sqlite3_column_text(stmt, 0);
                if (src) {
                    uint64_t nhash = cbm_fnv1a_64(src, strlen(src));
                    if (!cbm_visited_contains(visited, nhash)) {
                        if (q_tail >= q_cap) {
                            size_t new_cap = q_cap * 2;
                            char (*new_q)[CBM_URI_MAX_LEN] = (char (*)[CBM_URI_MAX_LEN])realloc(queue, new_cap * CBM_URI_MAX_LEN);
                            uint32_t *new_dq = (uint32_t *)realloc(depth_queue, new_cap * sizeof(uint32_t));
                            if (!new_q || !new_dq) {
                                if (new_q) queue = new_q;
                                if (new_dq) depth_queue = new_dq;
                                break;
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
            sqlite3_finalize(stmt);
        }
    }

    free(queue);
    free(depth_queue);
    return 0;
}

int cbm_trigger_recall(sqlite3 *db, const CbmUri *contested_root, const char *reason, RecallReport *out_report) {
    if (!contested_root || !out_report) return -1;

    memset(out_report, 0, sizeof(*out_report));
    out_report->contested_root = *contested_root;
    if (reason) {
        snprintf(out_report->reason, sizeof(out_report->reason), "%s", reason);
    }

    VisitedSet visited = cbm_visited_init();
    int rc = cbm_bfs_reverse_deps_acyclic(db, contested_root, &visited, CBM_MAX_TRAVERSAL_DEPTH, out_report);
    if (rc != 0) return rc;

    if (db) {
        sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);

        /* Mark contested_root as CONTESTED */
        char root_str[CBM_URI_MAX_LEN];
        cbm_uri_to_string(contested_root, root_str, sizeof(root_str));

        sqlite3_stmt *stmt = NULL;
        const char *sql = "UPDATE symbolic_nodes SET epistemic_status = 'CONTESTED' WHERE cbm_uri = ?";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, root_str, -1, SQLITE_STATIC);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);

            /* Mark all affected nodes as CONTESTED, reusing prepared statement */
            for (size_t i = 0; i < out_report->affected_count; i++) {
                sqlite3_bind_text(stmt, 1, out_report->affected_uris[i], -1, SQLITE_STATIC);
                sqlite3_step(stmt);
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);
        }

        sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    }

    return 0;
}
