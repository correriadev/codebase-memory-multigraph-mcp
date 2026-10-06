#include "mcp.h"
#include "mcp_internal.h"
#include "../admission/admission_gate.h"
#include "../core/cbm_uri.h"
#include "../foundation/platform.h"
#include "../foundation/log.h"
#include "../union/union_refusal.h"
#include "../union/union_contest.h"
#include <yyjson/yyjson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool promotion_json_u32(const yyjson_val *value, uint32_t *out_value) {
    if (!value || !out_value || !yyjson_is_int(value)) return false;
    if (yyjson_is_sint(value) && yyjson_get_sint(value) < 0) return false;

    uint64_t parsed = yyjson_get_uint(value);
    if (parsed > UINT32_MAX) return false;
    *out_value = (uint32_t)parsed;
    return true;
}

static bool promotion_json_u64(const yyjson_val *value, uint64_t *out_value) {
    if (!value || !out_value || !yyjson_is_int(value)) return false;
    if (yyjson_is_sint(value) && yyjson_get_sint(value) < 0) return false;
    *out_value = yyjson_get_uint(value);
    return true;
}

static bool promotion_json_string(const yyjson_val *value, char *out_value, size_t out_capacity) {
    if (!value || !out_value || out_capacity == 0 || !yyjson_is_str(value)) return false;

    size_t len = yyjson_get_len(value);
    const char *parsed = yyjson_get_str(value);
    if (!parsed || len == 0 || len >= out_capacity || memchr(parsed, '\0', len) != NULL) return false;

    memcpy(out_value, parsed, len);
    out_value[len] = '\0';
    return true;
}

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

    /* W03: Session and contest checks */
    CbmSessionRegistry *sessions = srv ? cbm_mcp_server_sessions(srv) : NULL;
    CbmContestRegistry *contests = srv ? cbm_mcp_server_contest_registry(srv) : NULL;

    if (contests && cbm_contest_is_blocked(contests, horizon_id) > 0) {
        cbm_refusal_emit(CBM_REFUSAL_CONTEST_UNPROVEN, horizon_id, "promotion blocked by active blocking contestation");
        char err_resp[512];
        snprintf(err_resp, sizeof(err_resp),
                 "{\"isError\":true,\"code\":\"CONTEST_BLOCKED\",\"message\":\"Promotion blocked by active blocking contestation\"}");
        free(horizon_id);
        return cbm_mcp_text_result(err_resp, true);
    }

    const CbmSessionHorizon *sh = sessions ? cbm_session_get(sessions, horizon_id) : NULL;
    if (!sh) {
        cbm_log(CBM_LOG_INFO, "union.promotion_bypass_session", "horizon_id", horizon_id, "reason", "legacy_or_federation_call", NULL);
    } else {
        if (sh->refusals > 0) {
            cbm_refusal_emit(CBM_REFUSAL_SCOPE_EXCEEDED, horizon_id, "promotion blocked by uncompensated gateway refusal");
            char err_resp[512];
            snprintf(err_resp, sizeof(err_resp),
                     "{\"isError\":true,\"code\":\"GATEWAY_BLOCKED\",\"message\":\"Promotion blocked by uncompensated gateway refusal\"}");
            free(horizon_id);
            return cbm_mcp_text_result(err_resp, true);
        }
    }

    /* Parse anchors from JSON */
    TwoTierAnchor *anchors = NULL;
    size_t anchor_count = 0;
    bool invalid_anchors = false;
    bool allocation_failed = false;

    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        invalid_anchors = true;
    } else {
        yyjson_val *root = yyjson_doc_get_root(doc);
        yyjson_val *arr = root && yyjson_is_obj(root) ? yyjson_obj_get(root, "anchors") : NULL;
        if (arr && !yyjson_is_arr(arr)) {
            invalid_anchors = true;
        }
        if (!invalid_anchors && arr) {
            size_t n = yyjson_arr_size(arr);
            if (n > 0) {
                anchors = (TwoTierAnchor *)calloc(n, sizeof(TwoTierAnchor));
                if (!anchors) {
                    allocation_failed = true;
                } else {
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

                            if (!promotion_json_string(v_file, anchors[anchor_count].file_path,
                                                       sizeof(anchors[anchor_count].file_path)) ||
                                !promotion_json_string(v_sym, anchors[anchor_count].symbol_name,
                                                       sizeof(anchors[anchor_count].symbol_name))) {
                                invalid_anchors = true;
                            }
                            uint32_t byte_start = 0;
                            uint32_t byte_len = 0;
                            uint64_t signature_hash = 0;
                            if (v_start && !promotion_json_u32(v_start, &byte_start)) {
                                invalid_anchors = true;
                            }
                            if (v_len && !promotion_json_u32(v_len, &byte_len)) {
                                invalid_anchors = true;
                            }
                            if (v_hash && !promotion_json_u64(v_hash, &signature_hash)) {
                                invalid_anchors = true;
                            }
                            if (v_text && !yyjson_is_str(v_text)) {
                                invalid_anchors = true;
                            }
                            size_t expected_len = v_text ? yyjson_get_len(v_text) : 0;
                            const char *expected_text = v_text ? yyjson_get_str(v_text) : NULL;
                            if (v_text &&
                                (expected_len >= sizeof(anchors[anchor_count].expected_text) ||
                                 memchr(expected_text, '\0', expected_len) != NULL)) {
                                invalid_anchors = true;
                            }
                            if (byte_len == 0 && v_text) {
                                byte_len = (uint32_t)expected_len;
                            }
                            if (byte_len > sizeof(anchors[anchor_count].expected_text) ||
                                (v_text && byte_len != expected_len)) {
                                invalid_anchors = true;
                            }
                            if (invalid_anchors) break;

                            anchors[anchor_count].byte_start = byte_start;
                            anchors[anchor_count].byte_len = byte_len;
                            anchors[anchor_count].ast_signature_hash = signature_hash;
                            if (v_text) {
                                memcpy(anchors[anchor_count].expected_text, expected_text, expected_len);
                                anchors[anchor_count].expected_text[expected_len] = '\0';
                            }
                            if (!v_hash && expected_len > 0) {
                                anchors[anchor_count].ast_signature_hash =
                                    cbm_fnv1a_64(anchors[anchor_count].expected_text, expected_len);
                            }
                            anchor_count++;
                        } else {
                            invalid_anchors = true;
                            break;
                        }
                    }
                }
            }
        }
        yyjson_doc_free(doc);
    }

    if (invalid_anchors || allocation_failed) {
        if (anchors) free(anchors);
        free(horizon_id);
        if (allocation_failed) {
            return cbm_mcp_text_result(
                "{\"isError\":true,\"code\":\"INTERNAL_ERROR\",\"message\":\"could not allocate promotion anchors\"}",
                true);
        }
        return cbm_mcp_text_result(
            "{\"isError\":true,\"code\":\"INVALID_PARAMS\",\"message\":\"invalid promotion anchor\"}",
            true);
    }

    /* Fallback local pool and gate if NULL provided so integrity checks cannot be bypassed. */
    HorizonConnectionPool local_pool;
    bool own_pool = false;
    if (!pool) {
        cbm_horizon_pool_init(&local_pool, cbm_resolve_cache_dir());
        pool = &local_pool;
        own_pool = true;
    }

    AdmissionGate local_gate;
    if (!gate) {
        cbm_admission_gate_init(&local_gate, "default", 1);
        gate = &local_gate;
    }

    char err_buf[512] = {0};
    char *arg_repo = cbm_mcp_get_string_arg(args_json, "repo_path");
    if (!arg_repo) arg_repo = cbm_mcp_get_string_arg(args_json, "repoPath");
    if (!arg_repo) arg_repo = cbm_mcp_get_string_arg(args_json, "project");

    char canonical_root[1024] = {0};
    const char *repo_root = NULL;
    if (arg_repo && arg_repo[0]) {
        snprintf(canonical_root, sizeof(canonical_root), "%s", arg_repo);
        for (char *p = canonical_root; *p; p++) {
            if (*p == '\\') *p = '/';
        }
#if !defined(_WIN32)
        if (((canonical_root[0] >= 'a' && canonical_root[0] <= 'z') ||
             (canonical_root[0] >= 'A' && canonical_root[0] <= 'Z')) &&
            canonical_root[1] == ':' && canonical_root[2] == '/') {
            char drive = canonical_root[0];
            if (drive >= 'A' && drive <= 'Z') drive += ('a' - 'A');
            char temp[1024];
            snprintf(temp, sizeof(temp), "/mnt/%c/%s", drive, canonical_root + 3);
            snprintf(canonical_root, sizeof(canonical_root), "%s", temp);
        }
#endif
        repo_root = canonical_root;
        free(arg_repo);
    } else {
        repo_root = cbm_mcp_server_session_root(srv);
    }

    sqlite3 *owned_base_db = NULL;
    sqlite3 *prev_base_db = NULL;
    char prev_project_id[64] = {0};

    char *arg_project = cbm_mcp_get_string_arg(args_json, "project");
    if (!arg_project) arg_project = cbm_mcp_get_string_arg(args_json, "projectName");
    if (!arg_project) arg_project = cbm_mcp_get_string_arg(args_json, "project_name");

    const char *proj_name = arg_project;
    if (!proj_name && srv) {
        proj_name = cbm_mcp_server_session_project(srv);
    }

    if (gate && proj_name && proj_name[0]) {
        const char *cdir = cbm_resolve_cache_dir();
        if (cdir && cdir[0]) {
            char base_path[1024];
            snprintf(base_path, sizeof(base_path), "%s/%s.db", cdir, proj_name);
            if (sqlite3_open_v2(base_path, &owned_base_db, SQLITE_OPEN_READWRITE, NULL) == SQLITE_OK) {
                prev_base_db = gate->base_db;
                snprintf(prev_project_id, sizeof(prev_project_id), "%s", gate->project_id);
                cbm_admission_gate_set_base_db(gate, owned_base_db);
                snprintf(gate->project_id, sizeof(gate->project_id), "%s", proj_name);
            }
        }
    }

    int rc = cbm_promote_horizon(gate, pool, repo_root, horizon_id, anchors, anchor_count, err_buf, sizeof(err_buf));
    if (owned_base_db) {
        cbm_admission_gate_set_base_db(gate, prev_base_db);
        snprintf(gate->project_id, sizeof(gate->project_id), "%s", prev_project_id);
        sqlite3_close_v2(owned_base_db);
    }
    if (arg_project) {
        free(arg_project);
    }
    if (anchors) {
        free(anchors);
    }
    if (own_pool) {
        cbm_horizon_pool_close_all(&local_pool);
    }

    if (rc != CBM_ADMISSION_OK) {
        const char *err_code = "PROMOTION_FAILED";
        if (rc == CBM_ADMISSION_ERR_ANCHOR_DRIFT) err_code = "ANCHOR_DRIFT";
        else if (rc == CBM_ADMISSION_ERR_CONCURRENT_CONFLICT) err_code = "CONCURRENT_CONFLICT";
        else if (rc == CBM_ADMISSION_ERR_HORIZON_NOT_FOUND) err_code = "HORIZON_NOT_FOUND";
        else if (rc == CBM_ADMISSION_ERR_INVALID_PARAMS) err_code = "INVALID_PARAMS";
        else if (rc == CBM_ADMISSION_ERR_BASE_UNAVAILABLE) err_code = "BASE_UNAVAILABLE";

        char err_resp[1024];
        snprintf(err_resp, sizeof(err_resp),
                 "{\"isError\":true,\"code\":\"%s\",\"message\":\"%s\"}",
                 err_code,
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
