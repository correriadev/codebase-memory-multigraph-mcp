#include "mcp/harness_handlers.h"
#include "harness/harness_v2.h"
#include "store/governance_store.h"
#include "foundation/platform.h"
#include "foundation/log.h"
#include "yyjson/yyjson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static char *hh_strdup(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

static char *wrap_tool_error_ex(const char *code,
                                const char *message,
                                const char *admission_status,
                                const char *manifest_id,
                                bool recorded,
                                bool idempotent_replay) {
    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
    yyjson_mut_doc_set_root(mdoc, mroot);
    yyjson_mut_obj_add_bool(mdoc, mroot, "ok", false);
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "error", code ? code : "HARNESS_ERROR");
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "message", message ? message : "");
    if (admission_status && admission_status[0]) {
        yyjson_mut_obj_add_strcpy(mdoc, mroot, "admission_status", admission_status);
    }
    if (manifest_id && manifest_id[0]) {
        yyjson_mut_obj_add_strcpy(mdoc, mroot, "manifest_id", manifest_id);
        yyjson_mut_obj_add_bool(mdoc, mroot, "recorded", recorded);
        yyjson_mut_obj_add_bool(mdoc, mroot, "idempotent_replay", idempotent_replay);
    }
    size_t len = 0;
    char *s = yyjson_mut_write(mdoc, 0, &len);
    yyjson_mut_doc_free(mdoc);
    char *res = cbm_mcp_text_result(s ? s : "{\"ok\":false}", true);
    free(s);
    return res;
}

static char *wrap_tool_error(const char *code, const char *message, const char *admission_status) {
    return wrap_tool_error_ex(code, message, admission_status, NULL, false, false);
}

static char *wrap_tool_success(const char *payload_json) {
    return cbm_mcp_text_result(payload_json ? payload_json : "{}", false);
}

static bool is_safe_project_id(const char *project) {
    if (!project || !project[0]) {
        return false;
    }
    for (const char *p = project; *p; p++) {
        char c = *p;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
        if (!ok) {
            return false;
        }
    }
    return strstr(project, "..") == NULL;
}

static char *resolve_harness_project(cbm_mcp_server_t *srv, yyjson_val *args) {
    if (args) {
        yyjson_val *pv = yyjson_obj_get(args, "project");
        if (pv && yyjson_is_str(pv)) {
            const char *ps = yyjson_get_str(pv);
            if (is_safe_project_id(ps)) {
                return hh_strdup(ps);
            }
            return NULL;
        }
    }
    if (srv) {
        const char *sp = cbm_mcp_server_session_project(srv);
        if (is_safe_project_id(sp)) {
            return hh_strdup(sp);
        }
        cbm_store_t *st = cbm_mcp_server_store(srv);
        if (st) {
            cbm_project_t *projs = NULL;
            int count = 0;
            if (cbm_store_list_projects(st, &projs, &count) == CBM_STORE_OK && count > 0) {
                char *first = hh_strdup(projs[0].name);
                cbm_store_free_projects(projs, count);
                return first;
            }
        }
    }
    return NULL;
}

static cbm_store_t *acquire_harness_store(cbm_mcp_server_t *srv, const char *project,
                                          bool *out_owned) {
    if (out_owned) {
        *out_owned = false;
    }
    if (srv && project && project[0]) {
        cbm_store_t *st = cbm_mcp_server_store(srv);
        if (st) {
            cbm_project_t pinfo = {0};
            if (cbm_store_get_project(st, project, &pinfo) == CBM_STORE_OK) {
                cbm_project_free_fields(&pinfo);
                return st;
            }
        }
    }
    const char *cache = cbm_resolve_cache_dir();
    if (!cache || !is_safe_project_id(project)) {
        return NULL;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.db", cache, project);
    cbm_store_t *opened = cbm_store_open_path_existing(path);
    if (opened && out_owned) {
        *out_owned = true;
    }
    return opened;
}

static cbm_gov_status_t open_project_gov_store(const char *project,
                                               cbm_gov_store_t **out_gov) {
    /* Always resolve the managed governance DB path for the validated project;
     * never accept arbitrary governance_db_path from MCP tool callers (Codex P1 #7). */
    return cbm_gov_store_open(NULL, project, out_gov);
}

static char *serialize_json_val_or_default(yyjson_val *val, const char *fallback) {
    if (!val) {
        return hh_strdup(fallback);
    }
    if (yyjson_is_str(val)) {
        const char *s = yyjson_get_str(val);
        return hh_strdup((s && s[0]) ? s : fallback);
    }
    char *canon = NULL;
    if (cbm_canonical_json_from_val(val, &canon) == 0 && canon) {
        return canon;
    }
    return hh_strdup(fallback);
}

char *handle_extract_harness_bundle(cbm_mcp_server_t *srv, const char *args_json) {
    yyjson_doc *adoc = args_json ? yyjson_read(args_json, strlen(args_json), 0) : NULL;
    yyjson_val *args = adoc ? yyjson_doc_get_root(adoc) : NULL;

    char *project = resolve_harness_project(srv, args);
    if (!project) {
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error("PROJECT_NOT_FOUND",
                               "No valid project specified and no default project found.", NULL);
    }

    bool owned_store = false;
    cbm_store_t *store = acquire_harness_store(srv, project, &owned_store);
    if (!store) {
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error("STORE_OPEN_FAILED",
                               "Failed to open project graph store.", NULL);
    }

    const char *task_id = args ? yyjson_get_str(yyjson_obj_get(args, "task_id")) : NULL;
    const char *change_kind = args ? yyjson_get_str(yyjson_obj_get(args, "change_kind")) : NULL;

    int k_hop = 1;
    yyjson_val *kh_val = args ? yyjson_obj_get(args, "k_hop") : NULL;
    if (kh_val && yyjson_is_int(kh_val)) {
        k_hop = (int)yyjson_get_int(kh_val);
    }
    if (!cbm_harness_validate_k_hop(k_hop)) {
        if (owned_store) cbm_store_close(store);
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error("INVALID_K_HOP",
                               "k_hop must be between 1 and 3 inclusive.", NULL);
    }

    /* Always use trusted server/host clock, never caller-supplied evaluation_time (Codex P1 #5) */
    int64_t eval_time = cbm_gov_now();

    yyjson_val *poles_val = args ? yyjson_obj_get(args, "poles") : NULL;
    if (!poles_val || !yyjson_is_arr(poles_val) || yyjson_arr_size(poles_val) == 0) {
        if (owned_store) cbm_store_close(store);
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error("INVALID_POLES",
                               "poles must be a non-empty array of symbol names or qualified names.", NULL);
    }

    int pole_count = (int)yyjson_arr_size(poles_val);
    const char **seed_poles = (const char **)calloc((size_t)pole_count, sizeof(char *));
    yyjson_arr_iter it;
    yyjson_arr_iter_init(poles_val, &it);
    yyjson_val *elem = NULL;
    int idx = 0;
    while ((elem = yyjson_arr_iter_next(&it)) != NULL && idx < pole_count) {
        if (yyjson_is_str(elem)) {
            seed_poles[idx++] = yyjson_get_str(elem);
        }
    }

    cbm_gov_store_t *gov = NULL;
    cbm_gov_status_t gst = open_project_gov_store(project, &gov);
    if (gst != CBM_GOV_OK || !gov) {
        free((void *)seed_poles);
        if (owned_store) cbm_store_close(store);
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error(cbm_gov_status_code_str(gst),
                               "Failed to open governance SQLite WAL store.", NULL);
    }

    char *bundle_json = NULL;
    char bundle_hash[65] = {0};
    char snapshot_id[65] = {0};
    int rc = cbm_harness_extract_bundle(store, gov, project,
                                        task_id ? task_id : "TASK-DEFAULT",
                                        change_kind ? change_kind : "pure_refactor",
                                        seed_poles, idx, k_hop, eval_time,
                                        &bundle_json, bundle_hash, snapshot_id);
    free((void *)seed_poles);
    if (owned_store) cbm_store_close(store);
    free(project);
    if (adoc) yyjson_doc_free(adoc);

    if (rc != 0 || !bundle_json) {
        const char *code = (rc < -1) ? cbm_gov_status_code_str((cbm_gov_status_t)rc) : "EXTRACTION_FAILED";
        char *err = wrap_tool_error(code, cbm_gov_store_last_error(gov), NULL);
        cbm_gov_store_close(gov);
        return err;
    }

    cbm_gov_store_close(gov);
    char *res = wrap_tool_success(bundle_json);
    free(bundle_json);
    return res;
}

char *handle_evaluate_fast_path(cbm_mcp_server_t *srv, const char *args_json) {
    yyjson_doc *adoc = args_json ? yyjson_read(args_json, strlen(args_json), 0) : NULL;
    yyjson_val *args = adoc ? yyjson_doc_get_root(adoc) : NULL;

    char *project = resolve_harness_project(srv, args);
    if (!project) {
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error("PROJECT_NOT_FOUND",
                               "No valid project specified and no default project found.", NULL);
    }

    bool owned_store = false;
    cbm_store_t *store = acquire_harness_store(srv, project, &owned_store);
    if (!store) {
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error("STORE_OPEN_FAILED",
                               "Failed to open project graph store.", NULL);
    }

    const char *change_kind = args ? yyjson_get_str(yyjson_obj_get(args, "change_kind")) : NULL;
    int64_t eval_time = cbm_gov_now();

    cbm_gov_store_t *gov = NULL;
    cbm_gov_status_t gst = open_project_gov_store(project, &gov);
    if (gst != CBM_GOV_OK || !gov) {
        if (owned_store) cbm_store_close(store);
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error(cbm_gov_status_code_str(gst),
                               "Failed to open governance SQLite WAL store.", NULL);
    }

    char *bundle_str = NULL;
    yyjson_val *bval = args ? yyjson_obj_get(args, "bundle") : NULL;
    if (!bval && args) {
        bval = yyjson_obj_get(args, "bundle_json");
    }
    if (bval) {
        bundle_str = serialize_json_val_or_default(bval, NULL);
    } else if (args && yyjson_obj_get(args, "poles")) {
        yyjson_val *poles_val = yyjson_obj_get(args, "poles");
        if (yyjson_is_arr(poles_val) && yyjson_arr_size(poles_val) > 0) {
            int pcount = (int)yyjson_arr_size(poles_val);
            const char **spoles = (const char **)calloc((size_t)pcount, sizeof(char *));
            yyjson_arr_iter it;
            yyjson_arr_iter_init(poles_val, &it);
            yyjson_val *el;
            int idx = 0;
            while ((el = yyjson_arr_iter_next(&it)) != NULL && idx < pcount) {
                if (yyjson_is_str(el)) {
                    spoles[idx++] = yyjson_get_str(el);
                }
            }
            char bhash[65], snap[65];
            cbm_harness_extract_bundle(store, gov, project, "TASK-EVAL",
                                       change_kind ? change_kind : "pure_refactor",
                                       spoles, idx, 1, eval_time,
                                       &bundle_str, bhash, snap);
            free((void *)spoles);
        }
    }

    if (!bundle_str) {
        cbm_gov_store_close(gov);
        if (owned_store) cbm_store_close(store);
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error("MISSING_BUNDLE",
                               "Provide either bundle/bundle_json or poles array.", NULL);
    }

    cbm_fast_path_verdict_t verdict = {0};
    char *verdict_json = NULL;
    int rc = cbm_harness_evaluate_fast_path(store, gov, project, bundle_str,
                                            change_kind, eval_time, &verdict, &verdict_json);
    free(bundle_str);
    cbm_fast_path_verdict_free(&verdict);
    cbm_gov_store_close(gov);
    if (owned_store) cbm_store_close(store);
    free(project);
    if (adoc) yyjson_doc_free(adoc);

    if (rc != 0 || !verdict_json) {
        return wrap_tool_error("FAST_PATH_EVAL_FAILED",
                               "Failed to evaluate fast-path eligibility.", NULL);
    }
    char *res = wrap_tool_success(verdict_json);
    free(verdict_json);
    return res;
}

char *handle_record_admission_manifest(cbm_mcp_server_t *srv, const char *args_json) {
    yyjson_doc *adoc = args_json ? yyjson_read(args_json, strlen(args_json), 0) : NULL;
    yyjson_val *args = adoc ? yyjson_doc_get_root(adoc) : NULL;

    char *project = resolve_harness_project(srv, args);
    if (!project) {
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error("PROJECT_NOT_FOUND",
                               "No valid project specified and no default project found.", NULL);
    }

    bool owned_store = false;
    cbm_store_t *store = acquire_harness_store(srv, project, &owned_store);
    if (!store) {
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error("STORE_OPEN_FAILED",
                               "Failed to open project graph store.", NULL);
    }

    cbm_gov_store_t *gov = NULL;
    cbm_gov_status_t gst = open_project_gov_store(project, &gov);
    if (gst != CBM_GOV_OK || !gov) {
        if (owned_store) cbm_store_close(store);
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error(cbm_gov_status_code_str(gst),
                               "Failed to open governance SQLite WAL store.", NULL);
    }

    int64_t now_epoch = cbm_gov_now();

    char live_graph_gen[64] = {0};
    cbm_store_generation(store, live_graph_gen, sizeof(live_graph_gen));

    char live_rules_rev[65] = {0};
    cbm_gov_status_t rst = cbm_gov_evaluate_active_rules(
        gov, now_epoch, NULL, NULL, live_rules_rev, NULL);
    if (rst != CBM_GOV_OK) {
        char *err = wrap_tool_error(cbm_gov_status_code_str(rst),
                                    cbm_gov_store_last_error(gov), NULL);
        cbm_gov_store_close(gov);
        if (owned_store) cbm_store_close(store);
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        return err;
    }

    /* Fail-closed Fast-Path verification: default to FALSE unless server-verified bundle
     * evaluation explicitly proves eligibility (Codex P1 #1). */
    bool server_fp_eligible = false;
    char live_cov_rev[65] = {0};
    yyjson_val *bval = args ? yyjson_obj_get(args, "bundle") : NULL;
    if (!bval && args) {
        bval = yyjson_obj_get(args, "bundle_json");
    }
    if (bval) {
        char *bstr = serialize_json_val_or_default(bval, NULL);
        if (bstr) {
            cbm_fast_path_verdict_t fpv = {0};
            if (cbm_harness_evaluate_fast_path(store, gov, project, bstr,
                                               NULL, now_epoch, &fpv, NULL) == 0) {
                server_fp_eligible = fpv.eligible;
                if (fpv.coverage_revision && fpv.coverage_revision[0]) {
                    snprintf(live_cov_rev, sizeof(live_cov_rev), "%s", fpv.coverage_revision);
                }
            }
            cbm_fast_path_verdict_free(&fpv);
            free(bstr);
        }
    }

    /* Always compute live project coverage revision if not already computed from bundle */
    if (!live_cov_rev[0]) {
        cbm_harness_compute_coverage_revision(store, project, NULL, 0,
                                              live_cov_rev, NULL, NULL);
    }

    const char *manifest_id = args ? yyjson_get_str(yyjson_obj_get(args, "manifest_id")) : NULL;
    const char *task_id = args ? yyjson_get_str(yyjson_obj_get(args, "task_id")) : NULL;
    const char *snapshot_id = args ? yyjson_get_str(yyjson_obj_get(args, "snapshot_id")) : NULL;
    const char *graph_gen = args ? yyjson_get_str(yyjson_obj_get(args, "graph_generation")) : NULL;
    const char *rules_rev = args ? yyjson_get_str(yyjson_obj_get(args, "rules_revision")) : NULL;
    const char *cov_rev = args ? yyjson_get_str(yyjson_obj_get(args, "coverage_revision")) : NULL;
    const char *bundle_hash = args ? yyjson_get_str(yyjson_obj_get(args, "bundle_hash")) : NULL;
    const char *adv_hash = args ? yyjson_get_str(yyjson_obj_get(args, "advisor_verdict_hash")) : NULL;

    char expected_snap[65] = {0};
    if (live_graph_gen[0] && live_rules_rev[0] && live_cov_rev[0]) {
        cbm_harness_compute_snapshot_id(live_graph_gen, live_rules_rev, live_cov_rev, expected_snap);
    }

    cbm_admission_manifest_t m = {0};
    m.manifest_id = (char *)manifest_id;
    m.task_id = (char *)task_id;
    m.project_id = project;
    m.snapshot_id = (char *)snapshot_id;
    m.graph_generation = (char *)(graph_gen ? graph_gen : live_graph_gen);
    m.rules_revision = (char *)(rules_rev ? rules_rev : live_rules_rev);
    m.coverage_revision = (char *)(cov_rev ? cov_rev : live_cov_rev);
    m.bundle_hash = (char *)bundle_hash;
    m.fast_path_granted = (args && yyjson_get_bool(yyjson_obj_get(args, "fast_path_granted"))) ? 1 : 0;
    m.fast_path_reasons_json = serialize_json_val_or_default(
        args ? yyjson_obj_get(args, "fast_path_reasons") : NULL, "[]");
    m.advisor_invoked = (args && yyjson_get_bool(yyjson_obj_get(args, "advisor_invoked"))) ? 1 : 0;
    m.advisor_verdict_hash = (char *)adv_hash;
    m.blocking_violations_json = serialize_json_val_or_default(
        args ? yyjson_obj_get(args, "blocking_violations") : NULL, "[]");
    m.warnings_json = serialize_json_val_or_default(
        args ? yyjson_obj_get(args, "warnings") : NULL, "[]");
    m.tdd_oracles_passed = (!args || !yyjson_obj_get(args, "tdd_oracles_passed") ||
                            yyjson_get_bool(yyjson_obj_get(args, "tdd_oracles_passed")))
                               ? 1
                               : 0;

    char adm_status[32] = {0};
    bool replay = false;
    cbm_gov_status_t st = cbm_gov_record_manifest(
        gov, &m,
        expected_snap[0] ? expected_snap : NULL,
        live_graph_gen[0] ? live_graph_gen : NULL,
        live_rules_rev[0] ? live_rules_rev : NULL,
        live_cov_rev[0] ? live_cov_rev : NULL,
        server_fp_eligible, now_epoch, adm_status, &replay);

    free(m.fast_path_reasons_json);
    free(m.blocking_violations_json);
    free(m.warnings_json);

    if (st != CBM_GOV_OK) {
        bool was_recorded = (st == CBM_GOV_ERR_BLOCKING_VIOLATIONS);
        char *err = wrap_tool_error_ex(cbm_gov_status_code_str(st),
                                       cbm_gov_store_last_error(gov),
                                       adm_status,
                                       was_recorded ? manifest_id : NULL,
                                       was_recorded,
                                       replay);
        cbm_gov_store_close(gov);
        if (owned_store) cbm_store_close(store);
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        return err;
    }

    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
    yyjson_mut_doc_set_root(mdoc, mroot);
    yyjson_mut_obj_add_bool(mdoc, mroot, "ok", true);
    yyjson_mut_obj_add_bool(mdoc, mroot, "recorded", true);
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "manifest_id", manifest_id ? manifest_id : "");
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "admission_status", adm_status);
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "snapshot_id", snapshot_id ? snapshot_id : "");
    yyjson_mut_obj_add_bool(mdoc, mroot, "idempotent_replay", replay);

    size_t out_len = 0;
    char *out_str = yyjson_mut_write(mdoc, 0, &out_len);
    yyjson_mut_doc_free(mdoc);
    cbm_gov_store_close(gov);
    if (owned_store) cbm_store_close(store);
    free(project);
    if (adoc) yyjson_doc_free(adoc);

    char *res = wrap_tool_success(out_str);
    free(out_str);
    return res;
}

char *handle_tradition_propose(cbm_mcp_server_t *srv, const char *args_json) {
    yyjson_doc *adoc = args_json ? yyjson_read(args_json, strlen(args_json), 0) : NULL;
    yyjson_val *args = adoc ? yyjson_doc_get_root(adoc) : NULL;

    char *project = resolve_harness_project(srv, args);
    if (!project) {
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error("PROJECT_NOT_FOUND",
                               "No valid project specified and no default project found.", NULL);
    }

    cbm_gov_store_t *gov = NULL;
    cbm_gov_status_t gst = open_project_gov_store(project, &gov);
    if (gst != CBM_GOV_OK || !gov) {
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        return wrap_tool_error(cbm_gov_status_code_str(gst),
                               "Failed to open governance SQLite WAL store.", NULL);
    }

    const char *action = args ? yyjson_get_str(yyjson_obj_get(args, "action")) : NULL;
    const char *rule_id = args ? yyjson_get_str(yyjson_obj_get(args, "rule_id")) : NULL;
    int64_t now_epoch = cbm_gov_now();

    /* Reject any agent attempt to review/promote via MCP unless host-authenticated operator authority is present */
    if (action && strcmp(action, "review") == 0) {
        const char *new_status = yyjson_get_str(yyjson_obj_get(args, "new_status"));
        const char *operator_id = yyjson_get_str(yyjson_obj_get(args, "operator_id"));
        const char *auth_token = yyjson_get_str(yyjson_obj_get(args, "operator_token"));
        const char *rationale = yyjson_get_str(yyjson_obj_get(args, "rationale"));
        const char *superseded_by = yyjson_get_str(yyjson_obj_get(args, "superseded_by"));
        int64_t interval_sec = 0;
        yyjson_val *iv_val = yyjson_obj_get(args, "review_interval_seconds");
        if (iv_val && yyjson_is_int(iv_val)) {
            interval_sec = yyjson_get_int(iv_val);
        }

        bool verified = cbm_mcp_server_verify_operator_authority(srv, operator_id, auth_token);
        const char *effective_reviewer = operator_id ? operator_id
                                                     : cbm_mcp_server_get_operator_identity(srv);

        cbm_gov_status_t st = cbm_gov_review_rule(
            gov, rule_id, new_status, effective_reviewer, verified,
            rationale, interval_sec, superseded_by, now_epoch);
        if (st != CBM_GOV_OK) {
            char *err = wrap_tool_error(cbm_gov_status_code_str(st),
                                        cbm_gov_store_last_error(gov), NULL);
            cbm_gov_store_close(gov);
            free(project);
            if (adoc) yyjson_doc_free(adoc);
            return err;
        }

        char new_rev[65] = {0};
        cbm_gov_get_state(gov, new_rev, sizeof(new_rev), NULL, 0, NULL, 0, NULL);

        yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
        yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
        yyjson_mut_doc_set_root(mdoc, mroot);
        yyjson_mut_obj_add_bool(mdoc, mroot, "ok", true);
        yyjson_mut_obj_add_strcpy(mdoc, mroot, "rule_id", rule_id ? rule_id : "");
        yyjson_mut_obj_add_strcpy(mdoc, mroot, "status", new_status ? new_status : "");
        yyjson_mut_obj_add_strcpy(mdoc, mroot, "rules_revision", new_rev);

        size_t len = 0;
        char *s = yyjson_mut_write(mdoc, 0, &len);
        yyjson_mut_doc_free(mdoc);
        cbm_gov_store_close(gov);
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        char *res = wrap_tool_success(s);
        free(s);
        return res;
    }

    const char *scope_glob = args ? yyjson_get_str(yyjson_obj_get(args, "scope_glob")) : NULL;
    const char *constraint_kind = args ? yyjson_get_str(yyjson_obj_get(args, "constraint_kind")) : NULL;
    const char *proposer_id = args ? yyjson_get_str(yyjson_obj_get(args, "proposer_id")) : NULL;
    const char *session_id = args ? yyjson_get_str(yyjson_obj_get(args, "origin_session_id")) : NULL;
    int64_t interval_sec = 86400 * 30;
    yyjson_val *iv_val = args ? yyjson_obj_get(args, "review_interval_seconds") : NULL;
    if (iv_val && yyjson_is_int(iv_val)) {
        interval_sec = yyjson_get_int(iv_val);
    }

    char *payload_str = serialize_json_val_or_default(
        args ? yyjson_obj_get(args, "payload") : NULL, NULL);
    if (!payload_str && args) {
        payload_str = serialize_json_val_or_default(yyjson_obj_get(args, "payload_json"), NULL);
    }
    char *evidence_str = serialize_json_val_or_default(
        args ? yyjson_obj_get(args, "evidence_refs") : NULL, "[]");

    bool replay = false;
    cbm_gov_status_t st = cbm_gov_propose_rule(
        gov, rule_id, scope_glob, constraint_kind, payload_str,
        proposer_id, session_id, evidence_str, interval_sec, now_epoch, &replay);

    free(payload_str);
    free(evidence_str);

    if (st != CBM_GOV_OK) {
        char *err = wrap_tool_error(cbm_gov_status_code_str(st),
                                    cbm_gov_store_last_error(gov), NULL);
        cbm_gov_store_close(gov);
        free(project);
        if (adoc) yyjson_doc_free(adoc);
        return err;
    }

    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
    yyjson_mut_doc_set_root(mdoc, mroot);
    yyjson_mut_obj_add_bool(mdoc, mroot, "ok", true);
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "rule_id", rule_id ? rule_id : "");
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "status", "UNVERIFIED_CANDIDATE");
    yyjson_mut_obj_add_bool(mdoc, mroot, "idempotent_replay", replay);

    size_t len = 0;
    char *s = yyjson_mut_write(mdoc, 0, &len);
    yyjson_mut_doc_free(mdoc);
    cbm_gov_store_close(gov);
    free(project);
    if (adoc) yyjson_doc_free(adoc);

    char *res = wrap_tool_success(s);
    free(s);
    return res;
}
