#include "mcp.h"
#include "mcp_internal.h"
#include "../core/horizon_pool.h"
#include "../foundation/platform.h"
#include <yyjson/yyjson.h>
#if defined(__has_include)
  #if __has_include(<sqlite3.h>)
    #include <sqlite3.h>
  #elif __has_include("sqlite3.h")
    #include "sqlite3.h"
  #elif __has_include("vendored/sqlite3/sqlite3.h")
    #include "vendored/sqlite3/sqlite3.h"
  #endif
#else
  #include <sqlite3.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
#endif

char *handle_create_horizon(cbm_mcp_server_t *srv, const char *args_json, HorizonConnectionPool *pool) {
    if (!args_json) {
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"INVALID_PARAMS\",\"message\":\"missing arguments\"}", true);
    }

    /* Fallback local pool if NULL */
    HorizonConnectionPool local_pool;
    bool own_pool = false;
    if (!pool) {
        cbm_horizon_pool_init(&local_pool, cbm_resolve_cache_dir());
        pool = &local_pool;
        own_pool = true;
    }

    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"INVALID_PARAMS\",\"message\":\"malformed JSON arguments\"}", true);
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"INVALID_PARAMS\",\"message\":\"arguments must be a JSON object\"}", true);
    }

    /* 1. Extract horizon_id */
    const char *h_id_str = NULL;
    yyjson_val *v_id = yyjson_obj_get(root, "horizon_id");
    if (!v_id) v_id = yyjson_obj_get(root, "horizonId");
    if (v_id && yyjson_is_str(v_id)) {
        h_id_str = yyjson_get_str(v_id);
    }

    uint32_t pid = 0;
#ifdef _WIN32
    pid = (uint32_t)GetCurrentProcessId();
#else
    pid = (uint32_t)getpid();
#endif

    char actual_id[CBM_HORIZON_ID_MAX] = {0};
    int rc = cbm_create_horizon(pool, pid, h_id_str, actual_id, sizeof(actual_id));
    if (rc != 0) {
        yyjson_doc_free(doc);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"HORIZON_CREATE_FAILED\",\"message\":\"Failed to create horizon SQLite database\"}", true);
    }

    /* 2. Open DB to insert nodes and edges */
    sqlite3 *hdb = NULL;
    rc = cbm_horizon_pool_get(pool, actual_id, &hdb);
    if (rc != 0 || !hdb) {
        yyjson_doc_free(doc);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"HORIZON_OPEN_FAILED\",\"message\":\"Failed to open horizon database handle\"}", true);
    }

    sqlite3_exec(hdb, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
    uint64_t now = (uint64_t)time(NULL);
    size_t nodes_count = 0;
    size_t edges_count = 0;

    /* 3. Insert symbolic_nodes */
    yyjson_val *nodes_arr = yyjson_obj_get(root, "nodes");
    if (nodes_arr && yyjson_is_arr(nodes_arr)) {
        const char *ins_node_sql =
            "INSERT OR REPLACE INTO symbolic_nodes "
            "(cbm_uri, label, epistemic_status, is_dangling, code_snippet, created_at) "
            "VALUES (?, ?, ?, ?, ?, ?);";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(hdb, ins_node_sql, -1, &stmt, NULL) == SQLITE_OK) {
            size_t idx, max;
            yyjson_val *item;
            yyjson_arr_foreach(nodes_arr, idx, max, item) {
                if (yyjson_is_obj(item)) {
                    yyjson_val *v_uri = yyjson_obj_get(item, "cbm_uri");
                    if (!v_uri) v_uri = yyjson_obj_get(item, "uri");
                    yyjson_val *v_lbl = yyjson_obj_get(item, "label");
                    yyjson_val *v_st = yyjson_obj_get(item, "epistemic_status");
                    if (!v_st) v_st = yyjson_obj_get(item, "status");
                    yyjson_val *v_dang = yyjson_obj_get(item, "is_dangling");
                    yyjson_val *v_snip = yyjson_obj_get(item, "code_snippet");

                    const char *uri_str = v_uri && yyjson_is_str(v_uri) ? yyjson_get_str(v_uri) : NULL;
                    if (!uri_str || !uri_str[0]) continue;

                    const char *lbl_str = v_lbl && yyjson_is_str(v_lbl) ? yyjson_get_str(v_lbl) : "Symbol";
                    const char *st_str = v_st && yyjson_is_str(v_st) ? yyjson_get_str(v_st) : "PROPOSED";
                    int dang_val = 1;
                    if (v_dang) {
                        if (yyjson_is_bool(v_dang)) dang_val = yyjson_get_bool(v_dang) ? 1 : 0;
                        else if (yyjson_is_int(v_dang)) dang_val = yyjson_get_int(v_dang);
                    }
                    const char *snip_str = v_snip && yyjson_is_str(v_snip) ? yyjson_get_str(v_snip) : NULL;

                    sqlite3_bind_text(stmt, 1, uri_str, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 2, lbl_str, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 3, st_str, -1, SQLITE_STATIC);
                    sqlite3_bind_int(stmt, 4, dang_val);
                    if (snip_str) {
                        sqlite3_bind_text(stmt, 5, snip_str, -1, SQLITE_STATIC);
                    } else {
                        sqlite3_bind_null(stmt, 5);
                    }
                    sqlite3_bind_int64(stmt, 6, (sqlite3_int64)now);

                    if (sqlite3_step(stmt) == SQLITE_DONE) {
                        nodes_count++;
                    }
                    sqlite3_reset(stmt);
                }
            }
            sqlite3_finalize(stmt);
        }
    }

    /* 4. Insert virtual_edges */
    yyjson_val *edges_arr = yyjson_obj_get(root, "edges");
    if (edges_arr && yyjson_is_arr(edges_arr)) {
        const char *ins_edge_sql =
            "INSERT OR REPLACE INTO virtual_edges "
            "(source_uri, target_uri, edge_type, origin_horizon, created_at) "
            "VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(hdb, ins_edge_sql, -1, &stmt, NULL) == SQLITE_OK) {
            size_t idx, max;
            yyjson_val *item;
            yyjson_arr_foreach(edges_arr, idx, max, item) {
                if (yyjson_is_obj(item)) {
                    yyjson_val *v_src = yyjson_obj_get(item, "source_uri");
                    if (!v_src) v_src = yyjson_obj_get(item, "source");
                    yyjson_val *v_tgt = yyjson_obj_get(item, "target_uri");
                    if (!v_tgt) v_tgt = yyjson_obj_get(item, "target");
                    yyjson_val *v_type = yyjson_obj_get(item, "edge_type");
                    if (!v_type) v_type = yyjson_obj_get(item, "type");

                    const char *src_str = v_src && yyjson_is_str(v_src) ? yyjson_get_str(v_src) : NULL;
                    const char *tgt_str = v_tgt && yyjson_is_str(v_tgt) ? yyjson_get_str(v_tgt) : NULL;
                    const char *type_str = v_type && yyjson_is_str(v_type) ? yyjson_get_str(v_type) : "DEPENDS_ON";

                    if (!src_str || !tgt_str) continue;

                    sqlite3_bind_text(stmt, 1, src_str, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 2, tgt_str, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 3, type_str, -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 4, actual_id, -1, SQLITE_STATIC);
                    sqlite3_bind_int64(stmt, 5, (sqlite3_int64)now);

                    if (sqlite3_step(stmt) == SQLITE_DONE) {
                        edges_count++;
                    }
                    sqlite3_reset(stmt);
                }
            }
            sqlite3_finalize(stmt);
        }
    }

    sqlite3_exec(hdb, "COMMIT;", NULL, NULL, NULL);
    yyjson_doc_free(doc);

    /* Invalidate handle so subsequent queries refresh cursor if needed */
    cbm_horizon_pool_invalidate(pool, actual_id);

    if (own_pool) {
        cbm_horizon_pool_close_all(&local_pool);
    }

    char resp[512];
    snprintf(resp, sizeof(resp),
             "{\"success\":true,\"horizon_id\":\"%s\",\"status\":\"ACTIVE\",\"nodes_count\":%zu,\"edges_count\":%zu}",
             actual_id, nodes_count, edges_count);
    return cbm_mcp_text_result(resp, false);
}
