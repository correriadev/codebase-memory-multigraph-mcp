#include "mcp.h"
#include "mcp_internal.h"
#include "../core/horizon_pool.h"
#include "../query/kway_merge.h"
#include "../core/symbolic_node.h"
#include <yyjson/yyjson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations of base handlers from mcp.c */
char *handle_search_graph(cbm_mcp_server_t *srv, const char *args);
char *handle_query_graph(cbm_mcp_server_t *srv, const char *args);
char *handle_trace_call_path(cbm_mcp_server_t *srv, const char *args);

/* Helper to parse active_horizons array from JSON args using yyjson */
int cbm_mcp_parse_active_horizons(const char *args_json, char horizons[][CBM_HORIZON_ID_MAX], size_t max_horizons, size_t *out_count) {
    if (!out_count) return -1;
    *out_count = 0;
    if (!args_json || !horizons || max_horizons == 0) return 0;

    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) return 0;

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return 0;
    }

    yyjson_val *arr = yyjson_obj_get(root, "active_horizons");
    if (!arr) {
        arr = yyjson_obj_get(root, "activeHorizons");
    }

    if (arr && yyjson_is_arr(arr)) {
        size_t idx, max;
        yyjson_val *item;
        yyjson_arr_foreach(arr, idx, max, item) {
            if (yyjson_is_str(item) && *out_count < max_horizons) {
                const char *s = yyjson_get_str(item);
                size_t len = strlen(s);
                if (len > 0 && len < CBM_HORIZON_ID_MAX) {
                    memcpy(horizons[*out_count], s, len + 1);
                    (*out_count)++;
                }
            }
        }
    }

    yyjson_doc_free(doc);
    return 0;
}

/* Helper to extract client pagination parameters (limit, skip/offset) */
static void cbm_mcp_extract_pagination(const char *args_json, uint64_t *out_skip, uint64_t *out_limit, uint64_t default_limit) {
    if (out_skip) *out_skip = 0;
    if (out_limit) *out_limit = default_limit;
    if (!args_json) return;

    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) return;

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (root && yyjson_is_obj(root)) {
        yyjson_val *v_skip = yyjson_obj_get(root, "skip");
        if (!v_skip) v_skip = yyjson_obj_get(root, "offset");
        if (v_skip && yyjson_is_int(v_skip)) {
            int64_t val = yyjson_get_sint(v_skip);
            if (val >= 0 && out_skip) *out_skip = (uint64_t)val;
        }

        yyjson_val *v_limit = yyjson_obj_get(root, "limit");
        if (v_limit && yyjson_is_int(v_limit)) {
            int64_t val = yyjson_get_sint(v_limit);
            if (val >= 0 && out_limit) *out_limit = (uint64_t)val;
        }
    }
    yyjson_doc_free(doc);
}

/* Helper to integrate overlays into content[0].text for standard MCP clients and LLMs */
static void cbm_mcp_integrate_overlay_into_content(yyjson_mut_doc *mdoc, yyjson_mut_val *root, const char *overlay_key, yyjson_mut_val *overlay_val) {
    if (!mdoc || !root || !overlay_key || !overlay_val) return;

    /* 1. Add to root object */
    yyjson_mut_obj_add_val(mdoc, root, overlay_key, overlay_val);

    /* 2. Add to structuredContent if present */
    yyjson_mut_val *structured = yyjson_mut_obj_get(root, "structuredContent");
    if (structured && yyjson_mut_is_obj(structured)) {
        yyjson_mut_val *copy_for_sc = yyjson_mut_val_mut_copy(mdoc, overlay_val);
        if (copy_for_sc) {
            yyjson_mut_obj_add_val(mdoc, structured, overlay_key, copy_for_sc);
        }
    }

    /* 3. Integrate into content[0].text for standard MCP clients and LLMs */
    yyjson_mut_val *content = yyjson_mut_obj_get(root, "content");
    if (content && yyjson_mut_is_arr(content) && yyjson_mut_arr_size(content) > 0) {
        yyjson_mut_val *item = yyjson_mut_arr_get_first(content);
        if (item && yyjson_mut_is_obj(item)) {
            yyjson_mut_val *tval = yyjson_mut_obj_get(item, "text");
            if (tval && yyjson_mut_is_str(tval)) {
                const char *orig_text = yyjson_mut_get_str(tval);
                yyjson_doc *inner_doc = orig_text ? yyjson_read(orig_text, strlen(orig_text), 0) : NULL;
                yyjson_val *inner_root = inner_doc ? yyjson_doc_get_root(inner_doc) : NULL;

                if (inner_root && yyjson_is_obj(inner_root)) {
                    /* orig_text is JSON object: add overlay directly into this JSON structure */
                    yyjson_mut_doc *idoc = yyjson_doc_mut_copy(inner_doc, NULL);
                    yyjson_doc_free(inner_doc);
                    if (idoc) {
                        yyjson_mut_val *iroot = yyjson_mut_doc_get_root(idoc);
                        yyjson_mut_val *copy_val = yyjson_val_mut_copy(idoc, (yyjson_val *)overlay_val);
                        if (copy_val) {
                            yyjson_mut_obj_add_val(idoc, iroot, overlay_key, copy_val);
                        }
                        char *new_text = yyjson_mut_write(idoc, 0, NULL);
                        yyjson_mut_doc_free(idoc);
                        if (new_text) {
                            yyjson_mut_obj_remove_str(item, "text");
                            yyjson_mut_obj_add_strcpy(mdoc, item, "text", new_text);
                            free(new_text);
                        }
                    }
                } else {
                    /* orig_text is plain text or markdown: append overlay section */
                    if (inner_doc) yyjson_doc_free(inner_doc);
                    yyjson_mut_doc *sdoc = yyjson_mut_doc_new(NULL);
                    yyjson_mut_val *sval = yyjson_val_mut_copy(sdoc, (yyjson_val *)overlay_val);
                    yyjson_mut_doc_set_root(sdoc, sval);
                    char *overlay_json = yyjson_mut_write(sdoc, 0, NULL);
                    yyjson_mut_doc_free(sdoc);

                    size_t orig_len = orig_text ? strlen(orig_text) : 0;
                    size_t over_len = overlay_json ? strlen(overlay_json) : 0;
                    size_t comb_len = orig_len + over_len + 256;
                    char *combined = (char *)malloc(comb_len);
                    if (combined) {
                        snprintf(combined, comb_len, "%s\n\n### Active Horizon Overlays (%s)\n```json\n%s\n```\n",
                                 orig_text ? orig_text : "", overlay_key, overlay_json ? overlay_json : "[]");
                        yyjson_mut_obj_remove_str(item, "text");
                        yyjson_mut_obj_add_strcpy(mdoc, item, "text", combined);
                        free(combined);
                    }
                    if (overlay_json) free(overlay_json);
                }
            }
        }
    }
}


/* Federated search_graph handler integrating base graph with active horizons overlays */
char *cbm_mcp_handle_federated_search_graph(cbm_mcp_server_t *srv, const char *args_json, HorizonConnectionPool *pool) {
    char horizons[16][CBM_HORIZON_ID_MAX];
    size_t horizon_count = 0;
    cbm_mcp_parse_active_horizons(args_json, horizons, 16, &horizon_count);

    /* If no active horizons specified, delegate directly to legacy base handler */
    if (horizon_count == 0 || !pool) {
        return handle_search_graph(srv, args_json);
    }

    /* Validate all active horizons exist before proceeding */
    for (size_t i = 0; i < horizon_count; i++) {
        sqlite3 *hdb = NULL;
        if (cbm_horizon_pool_get(pool, horizons[i], &hdb) != 0 || !hdb) {
            char err_resp[512];
            snprintf(err_resp, sizeof(err_resp),
                     "{\"isError\":true,\"code\":\"HORIZON_NOT_FOUND\",\"message\":\"Active horizon '%s' does not exist in storage\"}",
                     horizons[i]);
            return cbm_mcp_text_result(err_resp, true);
        }
    }

    /* Merge base and horizon overlays using K-Way merge */
    char *base_result = handle_search_graph(srv, args_json);
    if (!base_result) return NULL;

    /* If base_result is an error, return as is */
    if (strstr(base_result, "\"isError\":true") != NULL) {
        return base_result;
    }

    /* Extract client pagination parameters */
    uint64_t skip = 0, limit = 100;
    cbm_mcp_extract_pagination(args_json, &skip, &limit, 100);

    /* Initialize K-Way Merge for active horizons */
    KWayMergeContext ctx;
    cbm_kway_merge_init(&ctx, skip, limit);

    for (size_t i = 0; i < horizon_count; i++) {
        sqlite3 *hdb = NULL;
        if (cbm_horizon_pool_get(pool, horizons[i], &hdb) == 0 && hdb) {
            const char *sql = "SELECT cbm_uri, label || ':' || coalesce(code_snippet,'') FROM symbolic_nodes WHERE epistemic_status != 'CONTESTED' ORDER BY cbm_uri";
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(hdb, sql, -1, &stmt, NULL) == SQLITE_OK) {
                cbm_kway_merge_add_cursor(&ctx, stmt);
            }
        }
    }

    cbm_kway_merge_prime(&ctx);

    /* Collect overlay records from K-Way merge */
    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
    yyjson_mut_doc_set_root(mdoc, mroot);
    yyjson_mut_val *overlay_arr = yyjson_mut_arr(mdoc);

    MergeRecord rec = {0};
    bool has_more = (ctx.heap.count > 0);
    while (has_more && cbm_kway_merge_step(&ctx, &rec, &has_more) == 0) {
        if (!rec.key[0]) break;
        yyjson_mut_val *item = yyjson_mut_obj(mdoc);
        yyjson_mut_obj_add_strcpy(mdoc, item, "uri", rec.key);
        yyjson_mut_obj_add_strcpy(mdoc, item, "payload", rec.payload);
        yyjson_mut_obj_add_str(mdoc, item, "source", "HORIZON");
        yyjson_mut_arr_add_val(overlay_arr, item);
    }
    cbm_kway_merge_close(&ctx);

    /* Append overlay records into base result and integrate into content[0].text */
    yyjson_doc *bdoc = yyjson_read(base_result, strlen(base_result), 0);
    if (bdoc) {
        yyjson_mut_val *bm_root = yyjson_val_mut_copy(mdoc, yyjson_doc_get_root(bdoc));
        yyjson_doc_free(bdoc);
        if (bm_root && yyjson_mut_is_obj(bm_root)) {
            cbm_mcp_integrate_overlay_into_content(mdoc, bm_root, "active_horizon_overlays", overlay_arr);
            yyjson_mut_doc_set_root(mdoc, bm_root);
            char *json = yyjson_mut_write(mdoc, 0, NULL);
            yyjson_mut_doc_free(mdoc);
            free(base_result);
            return json;
        }
    }

    cbm_mcp_integrate_overlay_into_content(mdoc, mroot, "active_horizon_overlays", overlay_arr);
    char *json = yyjson_mut_write(mdoc, 0, NULL);
    yyjson_mut_doc_free(mdoc);
    free(base_result);
    return json;
}

/* Federated query_graph handler integrating Cypher query with active horizons */
char *cbm_mcp_handle_federated_query_graph(cbm_mcp_server_t *srv, const char *args_json, HorizonConnectionPool *pool) {
    char horizons[16][CBM_HORIZON_ID_MAX];
    size_t horizon_count = 0;
    cbm_mcp_parse_active_horizons(args_json, horizons, 16, &horizon_count);

    if (horizon_count == 0 || !pool) {
        return handle_query_graph(srv, args_json);
    }

    /* Validate all active horizons exist before proceeding */
    for (size_t i = 0; i < horizon_count; i++) {
        sqlite3 *hdb = NULL;
        if (cbm_horizon_pool_get(pool, horizons[i], &hdb) != 0 || !hdb) {
            char err_resp[512];
            snprintf(err_resp, sizeof(err_resp),
                     "{\"isError\":true,\"code\":\"HORIZON_NOT_FOUND\",\"message\":\"Active horizon '%s' does not exist in storage\"}",
                     horizons[i]);
            return cbm_mcp_text_result(err_resp, true);
        }
    }

    char *base_result = handle_query_graph(srv, args_json);
    if (!base_result) return NULL;
    if (strstr(base_result, "\"isError\":true") != NULL) {
        return base_result;
    }

    /* Extract client pagination parameters */
    uint64_t skip = 0, limit = 100;
    cbm_mcp_extract_pagination(args_json, &skip, &limit, 100);

    /* Initialize K-Way Merge for active horizons overlay */
    KWayMergeContext ctx;
    cbm_kway_merge_init(&ctx, skip, limit);

    for (size_t i = 0; i < horizon_count; i++) {
        sqlite3 *hdb = NULL;
        if (cbm_horizon_pool_get(pool, horizons[i], &hdb) == 0 && hdb) {
            const char *sql = "SELECT cbm_uri, label FROM symbolic_nodes WHERE epistemic_status != 'CONTESTED' ORDER BY cbm_uri";
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(hdb, sql, -1, &stmt, NULL) == SQLITE_OK) {
                cbm_kway_merge_add_cursor(&ctx, stmt);
            }
        }
    }

    cbm_kway_merge_prime(&ctx);

    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
    yyjson_mut_doc_set_root(mdoc, mroot);
    yyjson_mut_val *nodes_arr = yyjson_mut_arr(mdoc);
    yyjson_mut_val *edges_arr = yyjson_mut_arr(mdoc);

    MergeRecord rec = {0};
    bool has_more = (ctx.heap.count > 0);
    while (has_more && cbm_kway_merge_step(&ctx, &rec, &has_more) == 0) {
        if (!rec.key[0]) break;
        yyjson_mut_val *n = yyjson_mut_obj(mdoc);
        yyjson_mut_obj_add_strcpy(mdoc, n, "uri", rec.key);
        yyjson_mut_obj_add_strcpy(mdoc, n, "label", rec.payload);
        yyjson_mut_obj_add_str(mdoc, n, "source", "HORIZON");
        yyjson_mut_arr_add_val(nodes_arr, n);
    }
    cbm_kway_merge_close(&ctx);

    /* Collect virtual edges from active horizons */
    for (size_t i = 0; i < horizon_count; i++) {
        sqlite3 *hdb = NULL;
        if (cbm_horizon_pool_get(pool, horizons[i], &hdb) == 0 && hdb) {
            const char *esql = "SELECT source_uri, target_uri, edge_type, origin_horizon FROM virtual_edges";
            sqlite3_stmt *estmt = NULL;
            if (sqlite3_prepare_v2(hdb, esql, -1, &estmt, NULL) == SQLITE_OK) {
                while (sqlite3_step(estmt) == SQLITE_ROW) {
                    yyjson_mut_val *e = yyjson_mut_obj(mdoc);
                    yyjson_mut_obj_add_str(mdoc, e, "source", (const char *)sqlite3_column_text(estmt, 0));
                    yyjson_mut_obj_add_str(mdoc, e, "target", (const char *)sqlite3_column_text(estmt, 1));
                    yyjson_mut_obj_add_str(mdoc, e, "type", (const char *)sqlite3_column_text(estmt, 2));
                    yyjson_mut_obj_add_str(mdoc, e, "origin_horizon", (const char *)sqlite3_column_text(estmt, 3));
                    yyjson_mut_arr_add_val(edges_arr, e);
                }
                sqlite3_finalize(estmt);
            }
        }
    }

    yyjson_doc *bdoc = yyjson_read(base_result, strlen(base_result), 0);
    if (bdoc) {
        yyjson_mut_val *bm_root = yyjson_val_mut_copy(mdoc, yyjson_doc_get_root(bdoc));
        yyjson_doc_free(bdoc);
        if (bm_root && yyjson_mut_is_obj(bm_root)) {
            cbm_mcp_integrate_overlay_into_content(mdoc, bm_root, "horizon_nodes", nodes_arr);
            cbm_mcp_integrate_overlay_into_content(mdoc, bm_root, "horizon_edges", edges_arr);
            yyjson_mut_doc_set_root(mdoc, bm_root);
            char *json = yyjson_mut_write(mdoc, 0, NULL);
            yyjson_mut_doc_free(mdoc);
            free(base_result);
            return json;
        }
    }

    cbm_mcp_integrate_overlay_into_content(mdoc, mroot, "horizon_nodes", nodes_arr);
    cbm_mcp_integrate_overlay_into_content(mdoc, mroot, "horizon_edges", edges_arr);
    char *json = yyjson_mut_write(mdoc, 0, NULL);
    yyjson_mut_doc_free(mdoc);
    free(base_result);
    return json;
}

/* Federated trace_path handler integrating overlays */
char *cbm_mcp_handle_federated_trace_path(cbm_mcp_server_t *srv, const char *args_json, HorizonConnectionPool *pool) {
    char horizons[16][CBM_HORIZON_ID_MAX];
    size_t horizon_count = 0;
    cbm_mcp_parse_active_horizons(args_json, horizons, 16, &horizon_count);

    if (horizon_count == 0 || !pool) {
        return handle_trace_call_path(srv, args_json);
    }

    /* Validate all active horizons exist before proceeding */
    for (size_t i = 0; i < horizon_count; i++) {
        sqlite3 *hdb = NULL;
        if (cbm_horizon_pool_get(pool, horizons[i], &hdb) != 0 || !hdb) {
            char err_resp[512];
            snprintf(err_resp, sizeof(err_resp),
                     "{\"isError\":true,\"code\":\"HORIZON_NOT_FOUND\",\"message\":\"Active horizon '%s' does not exist in storage\"}",
                     horizons[i]);
            return cbm_mcp_text_result(err_resp, true);
        }
    }

    char *base_result = handle_trace_call_path(srv, args_json);
    if (!base_result) return NULL;
    if (strstr(base_result, "\"isError\":true") != NULL) {
        return base_result;
    }

    /* Extract client pagination parameters */
    uint64_t skip = 0, limit = 100;
    cbm_mcp_extract_pagination(args_json, &skip, &limit, 100);

    /* Stream virtual edges via K-Way merge */
    KWayMergeContext ctx;
    cbm_kway_merge_init(&ctx, skip, limit);

    for (size_t i = 0; i < horizon_count; i++) {
        sqlite3 *hdb = NULL;
        if (cbm_horizon_pool_get(pool, horizons[i], &hdb) == 0 && hdb) {
            const char *sql = "SELECT source_uri, target_uri || ':' || edge_type FROM virtual_edges ORDER BY source_uri";
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(hdb, sql, -1, &stmt, NULL) == SQLITE_OK) {
                cbm_kway_merge_add_cursor(&ctx, stmt);
            }
        }
    }

    cbm_kway_merge_prime(&ctx);

    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
    yyjson_mut_doc_set_root(mdoc, mroot);
    yyjson_mut_val *edges_arr = yyjson_mut_arr(mdoc);

    MergeRecord rec = {0};
    bool has_more = (ctx.heap.count > 0);
    while (has_more && cbm_kway_merge_step(&ctx, &rec, &has_more) == 0) {
        if (!rec.key[0]) break;
        yyjson_mut_val *e = yyjson_mut_obj(mdoc);
        yyjson_mut_obj_add_strcpy(mdoc, e, "source", rec.key);
        yyjson_mut_obj_add_strcpy(mdoc, e, "target_type", rec.payload);
        yyjson_mut_obj_add_str(mdoc, e, "source_type", "VIRTUAL_OVERLAY");
        yyjson_mut_arr_add_val(edges_arr, e);
    }
    cbm_kway_merge_close(&ctx);

    yyjson_doc *bdoc = yyjson_read(base_result, strlen(base_result), 0);
    if (bdoc) {
        yyjson_mut_val *bm_root = yyjson_val_mut_copy(mdoc, yyjson_doc_get_root(bdoc));
        yyjson_doc_free(bdoc);
        if (bm_root && yyjson_mut_is_obj(bm_root)) {
            cbm_mcp_integrate_overlay_into_content(mdoc, bm_root, "virtual_trace_overlays", edges_arr);
            yyjson_mut_doc_set_root(mdoc, bm_root);
            char *json = yyjson_mut_write(mdoc, 0, NULL);
            yyjson_mut_doc_free(mdoc);
            free(base_result);
            return json;
        }
    }

    cbm_mcp_integrate_overlay_into_content(mdoc, mroot, "virtual_trace_overlays", edges_arr);
    char *json = yyjson_mut_write(mdoc, 0, NULL);
    yyjson_mut_doc_free(mdoc);
    free(base_result);
    return json;
}

