#include "horizon_sync_handler.h"
#include "mcp.h"
#include "mcp_internal.h"
#include "../core/horizon_pool.h"
#include "../core/horizon_spec_parser.h"
#include "../admission/scope_validator.h"
#include "../admission/admission_gate.h"
#include "../store/store.h"
#include "../foundation/platform.h"
#include <yyjson/yyjson.h>
#if defined(__has_include)
  #if __has_include(<sqlite3.h>)
    #include <sqlite3.h>
  #elif __has_include("sqlite3.h")
    #include "sqlite3.h"
  #endif
#else
  #include <sqlite3.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *handle_sync_horizon_spec(cbm_mcp_server_t *srv, const char *args_json, HorizonConnectionPool *pool) {
    if (!args_json) {
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"INVALID_PARAMS\",\"message\":\"missing arguments\"}", true);
    }

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
    if (!h_id_str || !h_id_str[0]) {
        yyjson_doc_free(doc);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"INVALID_PARAMS\",\"message\":\"horizon_id required\"}", true);
    }

    /* 2. Extract file_path */
    const char *file_path_str = NULL;
    yyjson_val *v_fp = yyjson_obj_get(root, "file_path");
    if (!v_fp) v_fp = yyjson_obj_get(root, "filePath");
    if (v_fp && yyjson_is_str(v_fp)) {
        file_path_str = yyjson_get_str(v_fp);
    }
    if (!file_path_str || !file_path_str[0]) {
        yyjson_doc_free(doc);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"INVALID_PARAMS\",\"message\":\"file_path required\"}", true);
    }

    if (strstr(file_path_str, "..") || file_path_str[0] == '/' || file_path_str[0] == '\\' || strstr(file_path_str, ":\\") || strstr(file_path_str, ":/")) {
        yyjson_doc_free(doc);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"INVALID_PARAMS\",\"message\":\"Invalid parameter: path traversal or absolute path not allowed\"}", true);
    }

    /* 3. Extract content (optional) */
    const char *content_str = NULL;
    yyjson_val *v_cnt = yyjson_obj_get(root, "content");
    if (v_cnt && yyjson_is_str(v_cnt)) {
        content_str = yyjson_get_str(v_cnt);
    }

    /* 4. Extract project (optional) */
    const char *proj_str = NULL;
    yyjson_val *v_prj = yyjson_obj_get(root, "project");
    if (!v_prj) v_prj = yyjson_obj_get(root, "projectName");
    if (!v_prj) v_prj = yyjson_obj_get(root, "project_name");
    if (v_prj && yyjson_is_str(v_prj)) {
        proj_str = yyjson_get_str(v_prj);
    }

    /* 5. Get horizon database */
    sqlite3 *hdb = NULL;
    int rc = cbm_horizon_pool_get(pool, h_id_str, &hdb);
    if (rc != 0 || !hdb) {
        yyjson_doc_free(doc);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"HORIZON_NOT_FOUND\",\"message\":\"horizon not found\"}", true);
    }

    if (proj_str && proj_str[0] && cbm_horizon_bind_project(hdb, proj_str) != 0) {
        yyjson_doc_free(doc);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result("{\"code\":\"HORIZON_PROJECT_CONFLICT\",\"message\":\"Cannot bind horizon to this project\"}", true);
    }

    /* 6. Read content from disk if not provided */
    char *disk_content = NULL;
    const char *final_content = content_str;
    if (!final_content) {
        FILE *f = NULL;
        if (srv) {
            const char *repo_root = cbm_mcp_server_session_root(srv);
            if (repo_root && repo_root[0]) {
                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s/%s", repo_root, file_path_str);
                f = fopen(full_path, "rb");
            }
        }

        if (!f) {
            yyjson_doc_free(doc);
            if (own_pool) cbm_horizon_pool_close_all(&local_pool);
            return cbm_mcp_text_result("{\"isError\":true,\"code\":\"FILE_NOT_FOUND\",\"message\":\"failed to read file from disk\"}", true);
        }

        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz < 0) sz = 0;

        if (sz > 10 * 1024 * 1024) {
            fclose(f);
            yyjson_doc_free(doc);
            if (own_pool) cbm_horizon_pool_close_all(&local_pool);
            return cbm_mcp_text_result("{\"isError\":true,\"code\":\"FILE_TOO_LARGE\",\"message\":\"file exceeds 10MB limit\"}", true);
        }

        disk_content = (char *)malloc(sz + 1);
        if (!disk_content) {
            fclose(f);
            yyjson_doc_free(doc);
            if (own_pool) cbm_horizon_pool_close_all(&local_pool);
            return cbm_mcp_text_result("{\"isError\":true,\"code\":\"OUT_OF_MEMORY\",\"message\":\"out of memory reading file\"}", true);
        }

        size_t read_bytes = fread(disk_content, 1, sz, f);
        disk_content[read_bytes] = '\0';
        fclose(f);
        final_content = disk_content;
    }

    /* 7. Resolve repo_name */
    const char *repo_name = proj_str;
    if (!repo_name || !repo_name[0]) {
        if (srv) {
            repo_name = cbm_mcp_server_session_project(srv);
        }
    }
    if (!repo_name || !repo_name[0]) {
        repo_name = "default";
    }

    /* 8. Compile tactical spec into horizon */
    TacticalSpecCompileReport report;
    memset(&report, 0, sizeof(report));
    char err_buf[512] = {0};

    rc = cbm_compile_spec_to_horizon(hdb,
                                     h_id_str,
                                     repo_name,
                                     file_path_str,
                                     final_content,
                                     &report,
                                     err_buf,
                                     sizeof(err_buf));

    if (disk_content) {
        free(disk_content);
        disk_content = NULL;
    }

    if (rc != 0) {
        yyjson_doc_free(doc);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        char err_resp[1024];
        snprintf(err_resp, sizeof(err_resp),
                 "{\"isError\":true,\"code\":\"SPEC_COMPILE_FAILED\",\"message\":\"%s\"}",
                 err_buf[0] ? err_buf : "Failed to compile specification into horizon");
        return cbm_mcp_text_result(err_resp, true);
    }

    /* Invalidate handle so subsequent queries refresh cursor */
    cbm_horizon_pool_invalidate(pool, h_id_str);

    if (own_pool) {
        cbm_horizon_pool_close_all(&local_pool);
    }

    char resp[512];
    snprintf(resp, sizeof(resp),
             "{\"success\":true,\"horizon_id\":\"%s\",\"file_path\":\"%s\",\"nodes_compiled\":%zu,\"edges_compiled\":%zu,\"sections_indexed\":%zu}",
             h_id_str, file_path_str, report.nodes_compiled, report.edges_compiled, report.sections_indexed);

    yyjson_doc_free(doc);
    return cbm_mcp_text_result(resp, false);
}

char *handle_validate_scope_horizon(cbm_mcp_server_t *srv, const char *args_json, HorizonConnectionPool *pool) {
    if (!args_json) {
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"INVALID_PARAMS\",\"message\":\"missing arguments\"}", true);
    }

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
    if (!h_id_str || !h_id_str[0]) {
        yyjson_doc_free(doc);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"INVALID_PARAMS\",\"message\":\"horizon_id required\"}", true);
    }

    /* 2. Extract strict_connectivity (optional, default true) */
    bool strict_connectivity = true;
    yyjson_val *v_strict = yyjson_obj_get(root, "strict_connectivity");
    if (!v_strict) v_strict = yyjson_obj_get(root, "strictConnectivity");
    if (v_strict && yyjson_is_bool(v_strict)) {
        strict_connectivity = yyjson_get_bool(v_strict);
    }

    /* 3. Get horizon database */
    sqlite3 *hdb = NULL;
    int rc = cbm_horizon_pool_get(pool, h_id_str, &hdb);
    if (rc != 0 || !hdb) {
        yyjson_doc_free(doc);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"HORIZON_NOT_FOUND\",\"message\":\"horizon not found\"}", true);
    }

    bool check_conflicts = false;
    yyjson_val *v_check = yyjson_obj_get(root, "check_conflicts");
    if (!v_check) v_check = yyjson_obj_get(root, "checkConflicts");
    if (v_check && yyjson_is_bool(v_check)) {
        check_conflicts = yyjson_get_bool(v_check);
    }

    if (check_conflicts) {
        AdmissionGate gate;
        cbm_admission_gate_init(&gate, srv ? cbm_mcp_server_session_project(srv) : "default", 1);
        HorizonConflictReport creport;
        if (cbm_admission_gate_check_concurrent_conflicts(&gate, pool, h_id_str, NULL, 0, &creport) == CBM_ADMISSION_ERR_CONCURRENT_CONFLICT) {
            char resp[1024];
            snprintf(resp, sizeof(resp), "{\"isError\":true,\"code\":-32000,\"reason\":\"CONCURRENT_CONFLICT\",\"conflicting_horizon\":\"%s\",\"file_path\":\"%s\",\"is_semantic_only\":%s}", creport.conflicting_horizon, creport.conflicting_file, creport.is_semantic_only ? "true" : "false");
            yyjson_doc_free(doc);
            if (own_pool) cbm_horizon_pool_close_all(&local_pool);
            return cbm_mcp_text_result(resp, true);
        }
    }

    /* 4. Get base DB */
    cbm_store_t *store = srv ? cbm_mcp_server_store(srv) : NULL;
    sqlite3 *base_db = store ? (sqlite3 *)cbm_store_get_db(store) : NULL;
    sqlite3 *owned_base_db = NULL;
    if (!base_db && srv) {
        const char *proj_name = cbm_mcp_server_session_project(srv);
        if (proj_name && proj_name[0]) {
            const char *cdir = cbm_resolve_cache_dir();
            if (cdir && cdir[0]) {
                char base_path[1024];
                snprintf(base_path, sizeof(base_path), "%s/%s.db", cdir, proj_name);
                if (sqlite3_open_v2(base_path, &owned_base_db, SQLITE_OPEN_READONLY, NULL) == SQLITE_OK) {
                    base_db = owned_base_db;
                }
            }
        }
    }

    /* 5. Validate scope horizon */
    ScopeValidationReport report;
    memset(&report, 0, sizeof(report));
    char err_buf[512] = {0};

    rc = cbm_validate_scope_horizon(hdb, base_db, strict_connectivity, &report, err_buf, sizeof(err_buf));

    if (owned_base_db) {
        sqlite3_close_v2(owned_base_db);
        owned_base_db = NULL;
    }

    if (own_pool) {
        cbm_horizon_pool_close_all(&local_pool);
    }

    if (rc < 0) {
        yyjson_doc_free(doc);
        cbm_scope_validation_report_free(&report);
        char err_resp[1024];
        snprintf(err_resp, sizeof(err_resp),
                 "{\"isError\":true,\"code\":\"VALIDATION_FAILED\",\"message\":\"%s\"}",
                 err_buf[0] ? err_buf : "Scope validation execution failed");
        return cbm_mcp_text_result(err_resp, true);
    }

    if (report.isolated_nodes_count == 0 && report.unresolved_deps_count == 0) {
        char resp[512];
        snprintf(resp, sizeof(resp),
                 "{\"success\":true,\"horizon_id\":\"%s\",\"isolated_nodes_count\":0,\"unresolved_dependencies_count\":0,\"status\":\"VALID\"}",
                 h_id_str);
        cbm_scope_validation_report_free(&report);
        yyjson_doc_free(doc);
        return cbm_mcp_text_result(resp, false);
    }

    /* Invalid scope validation report */
    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);

    yyjson_mut_obj_add_bool(out_doc, out_root, "success", false);
    yyjson_mut_obj_add_str(out_doc, out_root, "horizon_id", h_id_str);
    yyjson_mut_obj_add_str(out_doc, out_root, "code", "SCOPE_VALIDATION_FAILED");

    yyjson_mut_val *iso_arr = yyjson_mut_arr(out_doc);
    for (size_t i = 0; i < report.isolated_nodes_count; i++) {
        yyjson_mut_arr_add_str(out_doc, iso_arr, report.isolated_nodes[i]);
    }
    yyjson_mut_obj_add_val(out_doc, out_root, "isolated_nodes", iso_arr);

    yyjson_mut_val *dep_arr = yyjson_mut_arr(out_doc);
    for (size_t i = 0; i < report.unresolved_deps_count; i++) {
        yyjson_mut_arr_add_str(out_doc, dep_arr, report.unresolved_deps[i]);
    }
    yyjson_mut_obj_add_val(out_doc, out_root, "unresolved_dependencies", dep_arr);

    yyjson_mut_obj_add_str(out_doc, out_root, "status", "INVALID");

    char *json_out = yyjson_mut_write(out_doc, 0, NULL);
    yyjson_mut_doc_free(out_doc);
    cbm_scope_validation_report_free(&report);
    yyjson_doc_free(doc);

    char *result = cbm_mcp_text_result(json_out, false);
    free(json_out);
    return result;
}

char *handle_check_horizon_conflicts(cbm_mcp_server_t *srv, const char *args_json, HorizonConnectionPool *pool) {
    if (!args_json) {
        return cbm_mcp_text_result("{\"isError\":true,\"code\":-32602,\"message\":\"missing arguments\"}", true);
    }
    char *horizon_id = cbm_mcp_get_string_arg(args_json, "horizon_id");
    if (!horizon_id) return cbm_mcp_text_result("{\"isError\":true,\"code\":-32602,\"message\":\"horizon_id required\"}", true);
    
    // validate against path traversal
    if (strstr(horizon_id, "/") || strstr(horizon_id, "\\") || strstr(horizon_id, "..")) {
        free(horizon_id);
        return cbm_mcp_text_result("{\"isError\":true,\"code\":-32602,\"message\":\"Invalid parameter: horizon_id\"}", true);
    }
    
    HorizonConnectionPool local_pool;
    bool own_pool = false;
    if (!pool) {
        cbm_horizon_pool_init(&local_pool, cbm_resolve_cache_dir());
        pool = &local_pool;
        own_pool = true;
    }
    
    AdmissionGate gate;
    cbm_admission_gate_init(&gate, srv ? cbm_mcp_server_session_project(srv) : "default", 1);
    
    HorizonConflictReport report;
    int rc = cbm_admission_gate_check_concurrent_conflicts(&gate, pool, horizon_id, NULL, 0, &report);
    
    char resp[1024];
    if (rc == CBM_ADMISSION_ERR_CONCURRENT_CONFLICT) {
        snprintf(resp, sizeof(resp), "{\"isError\":true,\"code\":-32000,\"reason\":\"CONCURRENT_CONFLICT\",\"conflicting_horizon\":\"%s\",\"file_path\":\"%s\",\"is_semantic_only\":%s}", report.conflicting_horizon, report.conflicting_file, report.is_semantic_only ? "true" : "false");
        free(horizon_id);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result(resp, true);
    } else if (rc == CBM_ADMISSION_OK) {
        snprintf(resp, sizeof(resp), "{\"conflicts\":[],\"status\":\"CLEAN\"}");
        free(horizon_id);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result(resp, false);
    } else {
        snprintf(resp, sizeof(resp), "{\"isError\":true,\"code\":-32603,\"message\":\"conflict check failed\"}");
        free(horizon_id);
        if (own_pool) cbm_horizon_pool_close_all(&local_pool);
        return cbm_mcp_text_result(resp, true);
    }
}
