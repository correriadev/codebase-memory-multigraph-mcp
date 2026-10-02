#include "mcp.h"
#include "mcp_internal.h"
#include "../core/horizon_pool.h"
#include "../foundation/platform.h"
#include "../foundation/compat_fs.h"
#include "../daemon/horizon_reaper.h"
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

static int compare_horizon_names(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

char *handle_list_horizons(cbm_mcp_server_t *srv, const char *args_json, HorizonConnectionPool *pool) {
    (void)srv;
    yyjson_doc *args = args_json ? yyjson_read(args_json, strlen(args_json), 0) : NULL;
    yyjson_val *root = args ? yyjson_doc_get_root(args) : NULL;
    const char *project = yyjson_get_str(yyjson_obj_get(root, "project"));
    const char *status = yyjson_get_str(yyjson_obj_get(root, "status"));
    if (!status) status = "ACTIVE";
    yyjson_val *vo = yyjson_obj_get(root, "offset"), *vl = yyjson_obj_get(root, "limit");
    int64_t offset = vo ? yyjson_get_sint(vo) : 0, limit = vl ? yyjson_get_sint(vl) : 50;
    if (!yyjson_is_obj(root) || !project || !project[0] ||
        (yyjson_obj_get(root, "status") && !yyjson_is_str(yyjson_obj_get(root, "status"))) ||
        (vo && !yyjson_is_int(vo)) || (vl && !yyjson_is_int(vl)) ||
        offset < 0 || limit < 1 || limit > 500 ||
        (strcmp(status, "ACTIVE") && strcmp(status, "PROMOTED") && strcmp(status, "DISCARDED") && strcmp(status, "ALL"))) {
        yyjson_doc_free(args);
        return cbm_mcp_text_result("{\"code\":\"INVALID_PARAMS\",\"message\":\"project required; valid status and integer paging required\"}", true);
    }
    char directory[CBM_PATH_MAX];
    const char *base = pool ? pool->base_dir : cbm_resolve_cache_dir();
    int n = snprintf(directory, sizeof(directory), "%s/horizons", base && base[0] ? base : ".");
    if (n < 0 || (size_t)n >= sizeof(directory)) {
        yyjson_doc_free(args);
        return cbm_mcp_text_result("{\"code\":\"CATALOG_UNAVAILABLE\"}", true);
    }
    cbm_dir_t *dir = cbm_opendir(directory);
    cbm_path_info_t info;
    if (!dir && cbm_path_info_utf8(directory, &info) != CBM_PATH_INFO_ABSENT) {
        yyjson_doc_free(args);
        return cbm_mcp_text_result("{\"code\":\"CATALOG_UNAVAILABLE\",\"message\":\"Cannot read horizon directory\"}", true);
    }
    char **names = NULL;
    size_t count = 0;
    bool allocation_failed = false;
    cbm_dirent_t *entry;
    while (dir && (entry = cbm_readdir(dir)) != NULL) {
        size_t len = strlen(entry->name);
        if (entry->is_dir || len <= 3 || len >= CBM_HORIZON_ID_MAX + 3 || strcmp(entry->name + len - 3, ".db")) continue;
        char **grown = realloc(names, (count + 1) * sizeof(*names));
        if (!grown) { allocation_failed = true; break; }
        names = grown;
        names[count] = strdup(entry->name);
        if (!names[count]) { allocation_failed = true; break; }
        count++;
    }
    if (dir) cbm_closedir(dir);
    if (allocation_failed) {
        for (size_t i = 0; i < count; i++) free(names[i]);
        free(names);
        yyjson_doc_free(args);
        return cbm_mcp_text_result("{\"code\":\"CATALOG_UNAVAILABLE\",\"message\":\"Allocation failed\"}", true);
    }
    if (count > 1) qsort(names, count, sizeof(*names), compare_horizon_names);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out = yyjson_mut_obj(doc), *items = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, out);
    yyjson_mut_obj_add_val(doc, out, "horizons", items);
    yyjson_mut_obj_add_strcpy(doc, out, "project", project);
    size_t total = 0, returned = 0, unreadable = 0;
    for (size_t i = 0; i < count; i++) {
        char path[CBM_PATH_MAX];
        n = snprintf(path, sizeof(path), "%s/%s", directory, names[i]);
        if (n < 0 || (size_t)n >= sizeof(path) || cbm_path_info_utf8(path, &info) != CBM_PATH_INFO_OK ||
            !info.is_regular || info.is_symlink) { unreadable++; continue; }
        sqlite3 *db = NULL;
        if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
            if (db) sqlite3_close_v2(db);
            unreadable++; continue;
        }
        sqlite3_busy_timeout(db, 1000);
        sqlite3_stmt *stmt = NULL;
        bool bound = false, matches = false;
        if (sqlite3_prepare_v2(db, "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='horizon_context'", -1, &stmt, NULL) != SQLITE_OK ||
            sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt); sqlite3_close_v2(db); unreadable++; continue;
        }
        bool has_binding_table = sqlite3_column_int(stmt, 0) > 0;
        sqlite3_finalize(stmt);
        stmt = NULL;
        if (has_binding_table) {
            if (sqlite3_prepare_v2(db, "SELECT project FROM horizon_context WHERE singleton=1", -1, &stmt, NULL) != SQLITE_OK ||
                sqlite3_step(stmt) != SQLITE_ROW) {
                sqlite3_finalize(stmt); sqlite3_close_v2(db); unreadable++; continue;
            }
            bound = true;
            matches = strcmp((const char *)sqlite3_column_text(stmt, 0), project) == 0;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
        if (!bound) {
            /* Legacy horizons: exact URI authority, never wildcard or guessed project. */
            const char *sql = "SELECT 1 FROM symbolic_nodes WHERE substr(cbm_uri,1,6)='cbm://' "
                              "AND substr(cbm_uri,7,instr(substr(cbm_uri,7),'/')-1)=? LIMIT 1";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
                sqlite3_close_v2(db); unreadable++; continue;
            }
            sqlite3_bind_text(stmt, 1, project, -1, SQLITE_TRANSIENT);
            int step = sqlite3_step(stmt);
            matches = step == SQLITE_ROW;
            if (step != SQLITE_ROW && step != SQLITE_DONE) unreadable++;
            sqlite3_finalize(stmt);
            stmt = NULL;
        }
        if (!matches) { sqlite3_close_v2(db); continue; }
        const char *sql = "SELECT horizon_id,status,client_pid,created_at,last_heartbeat,based_on_seq FROM horizon_metadata LIMIT 1";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK || sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt); sqlite3_close_v2(db); unreadable++; continue;
        }
        const char *stored_status = (const char *)sqlite3_column_text(stmt, 1);
        if (strcmp(status, "ALL") && strcmp(status, stored_status)) {
            sqlite3_finalize(stmt); sqlite3_close_v2(db); continue;
        }
        total++;
        if (total <= (uint64_t)offset || returned >= (uint64_t)limit) {
            sqlite3_finalize(stmt); sqlite3_close_v2(db); continue;
        }
        yyjson_mut_val *item = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_strcpy(doc, item, "horizon_id", (const char *)sqlite3_column_text(stmt, 0));
        yyjson_mut_obj_add_strcpy(doc, item, "status", stored_status);
        yyjson_mut_obj_add_strcpy(doc, item, "association_source", bound ? "explicit_project" : "legacy_node_uri");
        uint32_t pid = (uint32_t)sqlite3_column_int64(stmt, 2);
        yyjson_mut_obj_add_uint(doc, item, "client_pid", pid);
        yyjson_mut_obj_add_bool(doc, item, "owner_alive", cbm_is_pid_alive(pid));
        yyjson_mut_obj_add_int(doc, item, "created_at", sqlite3_column_int64(stmt, 3));
        yyjson_mut_obj_add_int(doc, item, "last_heartbeat", sqlite3_column_int64(stmt, 4));
        yyjson_mut_obj_add_strcpy(doc, item, "based_on_seq", (const char *)sqlite3_column_text(stmt, 5));
        sqlite3_finalize(stmt);
        stmt = NULL;
        if (sqlite3_prepare_v2(db, "SELECT cbm_uri,substr(code_snippet,1,1000),length(code_snippet)>1000 "
                                 "FROM symbolic_nodes WHERE label='FractalTemenos' ORDER BY cbm_uri LIMIT 1", -1, &stmt, NULL) == SQLITE_OK &&
            sqlite3_step(stmt) == SQLITE_ROW) {
            yyjson_mut_obj_add_strcpy(doc, item, "context_uri", (const char *)sqlite3_column_text(stmt, 0));
            const char *preview = (const char *)sqlite3_column_text(stmt, 1);
            if (preview) yyjson_mut_obj_add_strcpy(doc, item, "context_preview", preview);
            yyjson_mut_obj_add_bool(doc, item, "preview_truncated", sqlite3_column_int(stmt, 2) != 0);
        }
        sqlite3_finalize(stmt);
        sqlite3_close_v2(db);
        yyjson_mut_arr_add_val(items, item);
        returned++;
    }
    for (size_t i = 0; i < count; i++) free(names[i]);
    free(names);
    yyjson_mut_obj_add_uint(doc, out, "total", total);
    yyjson_mut_obj_add_uint(doc, out, "returned", returned);
    yyjson_mut_obj_add_int(doc, out, "offset", offset);
    yyjson_mut_obj_add_int(doc, out, "limit", limit);
    yyjson_mut_obj_add_bool(doc, out, "has_more", total > (uint64_t)offset + returned);
    yyjson_mut_obj_add_uint(doc, out, "unreadable_entries", unreadable);
    yyjson_mut_obj_add_bool(doc, out, "partial", unreadable > 0);
    char *json = yyjson_mut_write(doc, 0, NULL);
    char *result = cbm_mcp_text_result(json ? json : "{\"code\":\"CATALOG_SERIALIZE_FAILED\"}", json == NULL);
    free(json);
    yyjson_mut_doc_free(doc);
    yyjson_doc_free(args);
    return result;
}

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

    /* 2. Extract client_pid */
    uint32_t pid = 0;
    yyjson_val *v_pid = yyjson_obj_get(root, "client_pid");
    if (!v_pid) v_pid = yyjson_obj_get(root, "clientPid");
    if (v_pid && yyjson_is_int(v_pid)) {
        pid = (uint32_t)yyjson_get_int(v_pid);
    } else {
#ifdef _WIN32
        pid = (uint32_t)GetCurrentProcessId();
#else
        pid = (uint32_t)getpid();
#endif
    }

    /* 3. Extract based_on_seq */
    const char *seq_str = "0";
    char seq_buf[64] = {0};
    yyjson_val *v_seq = yyjson_obj_get(root, "based_on_seq");
    if (!v_seq) v_seq = yyjson_obj_get(root, "basedOnSeq");
    if (v_seq && yyjson_is_str(v_seq)) {
        seq_str = yyjson_get_str(v_seq);
    } else {
        cbm_store_t *store = srv ? cbm_mcp_server_store(srv) : NULL;
        if (store) {
            if (cbm_store_generation(store, seq_buf, sizeof(seq_buf)) == CBM_STORE_OK && seq_buf[0] != '\0') {
                seq_str = seq_buf;
            }
        }
    }

    const char *project = yyjson_get_str(yyjson_obj_get(root, "project"));
    /* Refuse a conflicting binding before create can reactivate/update metadata. */
    if (h_id_str && project && project[0]) {
        sqlite3 *existing = NULL;
        if (cbm_horizon_pool_get(pool, h_id_str, &existing) == 0 && existing) {
            sqlite3_stmt *binding = NULL;
            bool conflict = false;
            if (sqlite3_prepare_v2(existing, "SELECT project FROM horizon_context WHERE singleton=1", -1, &binding, NULL) == SQLITE_OK &&
                sqlite3_step(binding) == SQLITE_ROW) {
                conflict = strcmp((const char *)sqlite3_column_text(binding, 0), project) != 0;
            }
            sqlite3_finalize(binding);
            if (conflict) {
                yyjson_doc_free(doc);
                if (own_pool) cbm_horizon_pool_close_all(&local_pool);
                return cbm_mcp_text_result("{\"code\":\"HORIZON_PROJECT_CONFLICT\",\"message\":\"Horizon belongs to another project\"}", true);
            }
        }
    }
    char actual_id[CBM_HORIZON_ID_MAX] = {0};
    int rc = cbm_create_horizon(pool, pid, h_id_str, seq_str, actual_id, sizeof(actual_id));
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
    if (project && project[0] && cbm_horizon_bind_project(hdb, project) != 0) {
        sqlite3_exec(hdb, "ROLLBACK;", NULL, NULL, NULL);
        yyjson_doc_free(doc);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result("{\"code\":\"HORIZON_PROJECT_CONFLICT\",\"message\":\"Cannot bind horizon to this project\"}", true);
    }
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
             "{\"success\":true,\"horizon_id\":\"%s\",\"status\":\"ACTIVE\",\"client_pid\":%u,\"based_on_seq\":\"%s\",\"nodes_count\":%zu,\"edges_count\":%zu}",
             actual_id, pid, seq_str, nodes_count, edges_count);
    return cbm_mcp_text_result(resp, false);
}
