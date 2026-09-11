#include "mcp.h"
#include "../admission/admission_gate.h"
#include <yyjson/yyjson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *handle_promote_horizon(cbm_mcp_server_t *srv, const char *args_json, HorizonConnectionPool *pool, AdmissionGate *gate) {
    if (!args_json) {
        return cbm_mcp_text_result("{\"isError\":true,\"message\":\"missing arguments\"}", true);
    }

    char *horizon_id = cbm_mcp_get_string_arg(args_json, "horizon_id");
    if (!horizon_id) {
        horizon_id = cbm_mcp_get_string_arg(args_json, "horizonId");
    }
    if (!horizon_id) {
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"INVALID_PARAMS\",\"message\":\"horizon_id required\"}", true);
    }

    /* Fallback local pool and gate if NULL provided so integrity checks cannot be bypassed */
    HorizonConnectionPool local_pool;
    bool own_pool = false;
    if (!pool) {
        cbm_horizon_pool_init(&local_pool, NULL);
        pool = &local_pool;
        own_pool = true;
    }

    AdmissionGate local_gate;
    if (!gate) {
        cbm_admission_gate_init(&local_gate, "default", 1);
        gate = &local_gate;
    }
    if (srv && srv->store && gate && !gate->base_db) {
        cbm_admission_gate_set_base_db(gate, (sqlite3 *)cbm_store_get_db(srv->store));
    }

    /* Parse anchors from JSON */
    TwoTierAnchor *anchors = NULL;
    size_t anchor_count = 0;

    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (doc) {
        yyjson_val *root = yyjson_doc_get_root(doc);
        yyjson_val *arr = yyjson_obj_get(root, "anchors");
        if (arr && yyjson_is_arr(arr)) {
            size_t n = yyjson_arr_size(arr);
            if (n > 0) {
                anchors = (TwoTierAnchor *)calloc(n, sizeof(TwoTierAnchor));
                if (anchors) {
                    size_t idx, max;
                    yyjson_val *item;
                    yyjson_arr_foreach(arr, idx, max, item) {
                        if (yyjson_is_obj(item)) {
                            yyjson_val *v_file = yyjson_obj_get(item, "file_path");
                            if (!v_file) v_file = yyjson_obj_get(item, "filePath");
                            yyjson_val *v_sym = yyjson_obj_get(item, "symbol_name");
                            if (!v_sym) v_sym = yyjson_obj_get(item, "symbolName");
                            yyjson_val *v_start = yyjson_obj_get(item, "byte_start");
                            if (!v_start) v_start = yyjson_obj_get(item, "byteStart");
                            yyjson_val *v_len = yyjson_obj_get(item, "byte_len");
                            if (!v_len) v_len = yyjson_obj_get(item, "byteLen");
                            yyjson_val *v_hash = yyjson_obj_get(item, "ast_signature_hash");
                            if (!v_hash) v_hash = yyjson_obj_get(item, "astSignatureHash");
                            yyjson_val *v_text = yyjson_obj_get(item, "expected_text");
                            if (!v_text) v_text = yyjson_obj_get(item, "expectedText");

                            if (v_file && yyjson_is_str(v_file)) {
                                snprintf(anchors[anchor_count].file_path, sizeof(anchors[anchor_count].file_path), "%s", yyjson_get_str(v_file));
                            }
                            if (v_sym && yyjson_is_str(v_sym)) {
                                snprintf(anchors[anchor_count].symbol_name, sizeof(anchors[anchor_count].symbol_name), "%s", yyjson_get_str(v_sym));
                            }
                            if (v_start && yyjson_is_int(v_start)) {
                                anchors[anchor_count].byte_start = (uint32_t)yyjson_get_uint(v_start);
                            }
                            if (v_len && yyjson_is_int(v_len)) {
                                anchors[anchor_count].byte_len = (uint32_t)yyjson_get_uint(v_len);
                            }
                            if (v_text && yyjson_is_str(v_text)) {
                                snprintf(anchors[anchor_count].expected_text, sizeof(anchors[anchor_count].expected_text), "%s", yyjson_get_str(v_text));
                                if (anchors[anchor_count].byte_len == 0) {
                                    anchors[anchor_count].byte_len = (uint32_t)strlen(anchors[anchor_count].expected_text);
                                }
                            }
                            if (v_hash && yyjson_is_int(v_hash)) {
                                anchors[anchor_count].ast_signature_hash = yyjson_get_uint(v_hash);
                            } else if (anchors[anchor_count].expected_text[0]) {
                                anchors[anchor_count].ast_signature_hash = cbm_fnv1a_64(anchors[anchor_count].expected_text, anchors[anchor_count].byte_len);
                            }
                            anchor_count++;
                        }
                    }
                }
            }
        }
        yyjson_doc_free(doc);
    }

    char err_buf[512] = {0};
    const char *repo_root = srv ? srv->session_root : NULL;

    int rc = cbm_promote_horizon(gate, pool, repo_root, horizon_id, anchors, anchor_count, err_buf, sizeof(err_buf));
    if (anchors) {
        free(anchors);
    }
    if (own_pool) {
        cbm_horizon_pool_close_all(&local_pool);
    }

    if (rc != CBM_ADMISSION_OK) {
        char err_resp[1024];
        snprintf(err_resp, sizeof(err_resp),
                 "{\"isError\":true,\"code\":\"%s\",\"message\":\"%s\"}",
                 (rc == CBM_ADMISSION_ERR_ANCHOR_DRIFT) ? "ANCHOR_DRIFT" : "PROMOTION_FAILED",
                 err_buf[0] ? err_buf : "Admission gate rejected promotion");
        free(horizon_id);
        return cbm_mcp_text_result(err_resp, true);
    }

    char success_resp[512];
    snprintf(success_resp, sizeof(success_resp),
             "{\"promoted\":true,\"horizon_id\":\"%s\",\"status\":\"PROMOTED\"}",
             horizon_id);
    free(horizon_id);
    return cbm_mcp_text_result(success_resp, false);
}
