#include "symbolic_node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *status_to_str(EpistemicStatus st) {
    switch (st) {
        case EPISTEMIC_PROPOSED: return "PROPOSED";
        case EPISTEMIC_ACCEPTED: return "ACCEPTED";
        case EPISTEMIC_CONTESTED: return "CONTESTED";
        case EPISTEMIC_SHADOWED: return "SHADOWED";
        default: return "PROPOSED";
    }
}

static EpistemicStatus str_to_status(const char *s) {
    if (!s) return EPISTEMIC_PROPOSED;
    if (strcmp(s, "ACCEPTED") == 0) return EPISTEMIC_ACCEPTED;
    if (strcmp(s, "CONTESTED") == 0) return EPISTEMIC_CONTESTED;
    if (strcmp(s, "SHADOWED") == 0) return EPISTEMIC_SHADOWED;
    return EPISTEMIC_PROPOSED;
}

static const char *edge_type_to_str(EdgeType t) {
    switch (t) {
        case EDGE_VIRTUAL_CALLS: return "VIRTUAL_CALLS";
        case EDGE_REPLACES: return "REPLACES";
        case EDGE_EXTENDS: return "EXTENDS";
        case EDGE_REFERENCES: return "REFERENCES";
        default: return "REFERENCES";
    }
}

static EdgeType str_to_edge_type(const char *s) {
    if (!s) return EDGE_REFERENCES;
    if (strcmp(s, "VIRTUAL_CALLS") == 0) return EDGE_VIRTUAL_CALLS;
    if (strcmp(s, "REPLACES") == 0) return EDGE_REPLACES;
    if (strcmp(s, "EXTENDS") == 0) return EDGE_EXTENDS;
    return EDGE_REFERENCES;
}

int cbm_symbolic_node_insert(sqlite3 *db, const SymbolicNode *node, const char *code_snippet) {
    if (!db || !node) return -1;

    char uri_str[CBM_URI_MAX_LEN];
    if (cbm_uri_to_string(&node->uri, uri_str, sizeof(uri_str)) != 0) {
        return -1;
    }

    const char *sql =
        "INSERT OR REPLACE INTO symbolic_nodes (cbm_uri, label, epistemic_status, is_dangling, code_snippet, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    uint64_t now = (uint64_t)time(NULL);
    sqlite3_bind_text(stmt, 1, uri_str, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, node->label, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, status_to_str(node->status), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, node->is_dangling ? 1 : 0);
    if (code_snippet) {
        sqlite3_bind_text(stmt, 5, code_snippet, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 5);
    }
    sqlite3_bind_int64(stmt, 6, (sqlite3_int64)now);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int cbm_symbolic_node_find(sqlite3 *db, const char *cbm_uri_str, SymbolicNode *out_node, char *out_snippet, size_t snippet_sz) {
    if (!db || !cbm_uri_str || !out_node) return -1;

    const char *sql = "SELECT cbm_uri, label, epistemic_status, is_dangling, code_snippet FROM symbolic_nodes WHERE cbm_uri = ?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, cbm_uri_str, -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    const char *uri_text = (const char *)sqlite3_column_text(stmt, 0);
    cbm_uri_parse(uri_text, &out_node->uri);

    const char *label_text = (const char *)sqlite3_column_text(stmt, 1);
    snprintf(out_node->label, sizeof(out_node->label), "%s", label_text ? label_text : "");

    const char *status_text = (const char *)sqlite3_column_text(stmt, 2);
    out_node->status = str_to_status(status_text);
    out_node->is_dangling = sqlite3_column_int(stmt, 3) != 0;

    if (out_snippet && snippet_sz > 0) {
        const char *snip = (const char *)sqlite3_column_text(stmt, 4);
        snprintf(out_snippet, snippet_sz, "%s", snip ? snip : "");
    }

    sqlite3_finalize(stmt);
    return 0;
}

int cbm_virtual_edge_insert(sqlite3 *db, const VirtualEdge *edge) {
    if (!db || !edge) return -1;

    char src[CBM_URI_MAX_LEN], tgt[CBM_URI_MAX_LEN];
    cbm_uri_to_string(&edge->source, src, sizeof(src));
    cbm_uri_to_string(&edge->target, tgt, sizeof(tgt));

    const char *sql =
        "INSERT OR REPLACE INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) "
        "VALUES (?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    uint64_t now = (uint64_t)time(NULL);
    sqlite3_bind_text(stmt, 1, src, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, tgt, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, edge_type_to_str(edge->type), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, edge->origin_horizon, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, (sqlite3_int64)now);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int cbm_virtual_edge_find_outbound(sqlite3 *db, const char *source_uri, VirtualEdge *out_edges, size_t max_edges, size_t *out_count) {
    if (!db || !source_uri || !out_edges || !out_count) return -1;
    *out_count = 0;

    const char *sql = "SELECT source_uri, target_uri, edge_type, origin_horizon FROM virtual_edges WHERE source_uri = ?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, source_uri, -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW && *out_count < max_edges) {
        VirtualEdge *e = &out_edges[*out_count];
        cbm_uri_parse((const char *)sqlite3_column_text(stmt, 0), &e->source);
        cbm_uri_parse((const char *)sqlite3_column_text(stmt, 1), &e->target);
        e->type = str_to_edge_type((const char *)sqlite3_column_text(stmt, 2));
        snprintf(e->origin_horizon, sizeof(e->origin_horizon), "%s", (const char *)sqlite3_column_text(stmt, 3));
        (*out_count)++;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int cbm_virtual_edge_find_inbound(sqlite3 *db, const char *target_uri, VirtualEdge *out_edges, size_t max_edges, size_t *out_count) {
    if (!db || !target_uri || !out_edges || !out_count) return -1;
    *out_count = 0;

    const char *sql = "SELECT source_uri, target_uri, edge_type, origin_horizon FROM virtual_edges WHERE target_uri = ?";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, target_uri, -1, SQLITE_STATIC);
    while (sqlite3_step(stmt) == SQLITE_ROW && *out_count < max_edges) {
        VirtualEdge *e = &out_edges[*out_count];
        cbm_uri_parse((const char *)sqlite3_column_text(stmt, 0), &e->source);
        cbm_uri_parse((const char *)sqlite3_column_text(stmt, 1), &e->target);
        e->type = str_to_edge_type((const char *)sqlite3_column_text(stmt, 2));
        snprintf(e->origin_horizon, sizeof(e->origin_horizon), "%s", (const char *)sqlite3_column_text(stmt, 3));
        (*out_count)++;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int cbm_traverse_symbolic_bfs(sqlite3 *db, const char *start_uri, bool reverse, VisitedSet *visited, uint32_t max_depth, char out_visited_uris[][CBM_URI_MAX_LEN], size_t max_out, size_t *out_count) {
    if (!db || !start_uri || !visited || !out_visited_uris || !out_count) return -1;
    *out_count = 0;

    size_t q_cap = 512;
    char (*queue)[CBM_URI_MAX_LEN] = (char (*)[CBM_URI_MAX_LEN])malloc(q_cap * CBM_URI_MAX_LEN);
    uint32_t *depth_queue = (uint32_t *)malloc(q_cap * sizeof(uint32_t));
    if (!queue || !depth_queue) {
        if (queue) free(queue);
        if (depth_queue) free(depth_queue);
        return -1;
    }

    size_t q_head = 0, q_tail = 0;

    snprintf(queue[q_tail], CBM_URI_MAX_LEN, "%s", start_uri);
    depth_queue[q_tail] = 0;
    q_tail++;

    while (q_head < q_tail) {
        char current[CBM_URI_MAX_LEN];
        snprintf(current, sizeof(current), "%s", queue[q_head]);
        uint32_t current_depth = depth_queue[q_head];
        q_head++;

        uint64_t hash = cbm_fnv1a_64(current, strlen(current));
        if (cbm_visited_contains_uri(visited, hash, current)) {
            continue;
        }
        if (!cbm_visited_add_uri(visited, hash, current)) {
            break;
        }

        if (*out_count < max_out) {
            snprintf(out_visited_uris[*out_count], CBM_URI_MAX_LEN, "%s", current);
            (*out_count)++;
        }

        if (current_depth >= max_depth) {
            continue;
        }

        const char *sql = reverse ?
            "SELECT source_uri FROM virtual_edges WHERE target_uri = ?" :
            "SELECT target_uri FROM virtual_edges WHERE source_uri = ?";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, current, -1, SQLITE_STATIC);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char *next_uri = (const char *)sqlite3_column_text(stmt, 0);
                if (!next_uri) continue;
                uint64_t nhash = cbm_fnv1a_64(next_uri, strlen(next_uri));
                if (!cbm_visited_contains_uri(visited, nhash, next_uri)) {
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
                    snprintf(queue[q_tail], CBM_URI_MAX_LEN, "%s", next_uri);
                    depth_queue[q_tail] = current_depth + 1;
                    q_tail++;
                }
            }
            sqlite3_finalize(stmt);
        }
    }

    free(queue);
    free(depth_queue);
    return 0;
}
