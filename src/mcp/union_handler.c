#include "union_handler.h"
#include "mcp.h"
#include "mcp_internal.h"
#include "../foundation/log.h"
#include "../foundation/platform.h"
#include <yyjson/yyjson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
#endif

/* Fallback registries when srv == NULL (e.g. testing in isolation) */
static CbmSessionRegistry s_fallback_sessions;
static CbmContractRegistry s_fallback_contracts;
static CbmGateway s_fallback_gateway;
static CbmThemeRegistry s_fallback_themes;
static CbmBindingLedger s_fallback_bindings;
static CbmContestRegistry s_fallback_contests;
static HorizonConnectionPool s_fallback_pool;
static bool s_fallback_inited = false;
static uint64_t s_contest_seq = 0;

static void ensure_fallback_init(void) {
    if (!s_fallback_inited) {
        cbm_session_registry_init(&s_fallback_sessions);
        cbm_contract_registry_init(&s_fallback_contracts);
        cbm_gateway_init(&s_fallback_gateway);
        cbm_theme_registry_init(&s_fallback_themes);
        cbm_binding_ledger_init(&s_fallback_bindings);
        cbm_contest_registry_init(&s_fallback_contests);
        cbm_horizon_pool_init(&s_fallback_pool, cbm_resolve_cache_dir());
        s_fallback_inited = true;
    }
}

static CbmSessionRegistry *get_sessions(cbm_mcp_server_t *srv) {
    ensure_fallback_init();
    return srv ? cbm_mcp_server_sessions(srv) : &s_fallback_sessions;
}

static CbmContractRegistry *get_contracts(cbm_mcp_server_t *srv) {
    ensure_fallback_init();
    return srv ? cbm_mcp_server_contracts(srv) : &s_fallback_contracts;
}

static CbmGateway *get_gateway(cbm_mcp_server_t *srv) {
    ensure_fallback_init();
    return srv ? cbm_mcp_server_gateway(srv) : &s_fallback_gateway;
}

static CbmThemeRegistry *get_themes(cbm_mcp_server_t *srv) {
    ensure_fallback_init();
    return srv ? cbm_mcp_server_theme_registry(srv) : &s_fallback_themes;
}

static CbmBindingLedger *get_bindings(cbm_mcp_server_t *srv) {
    ensure_fallback_init();
    return srv ? cbm_mcp_server_binding_ledger(srv) : &s_fallback_bindings;
}

static CbmContestRegistry *get_contests(cbm_mcp_server_t *srv) {
    ensure_fallback_init();
    return srv ? cbm_mcp_server_contest_registry(srv) : &s_fallback_contests;
}

static HorizonConnectionPool *get_pool(cbm_mcp_server_t *srv) {
    ensure_fallback_init();
    return srv ? cbm_mcp_server_horizon_pool(srv) : &s_fallback_pool;
}

/* Helper to convert yyjson_mut_doc to cbm_mcp_text_result safely */
static char *result_from_mut_doc(yyjson_mut_doc *doc, bool is_error) {
    if (!doc) {
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"INTERNAL_ERROR\",\"message\":\"failed to allocate JSON response\"}", true);
    }
    char *json = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);
    if (!json) {
        return cbm_mcp_text_result("{\"isError\":true,\"code\":\"INTERNAL_ERROR\",\"message\":\"failed to serialize JSON response\"}", true);
    }
    char *res = cbm_mcp_text_result(json, is_error);
    free(json);
    return res;
}

static char *json_error_result(const char *code, const char *message) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_bool(doc, root, "isError", true);
    yyjson_mut_obj_add_strcpy(doc, root, "code", code ? code : "ERROR");
    yyjson_mut_obj_add_strcpy(doc, root, "message", message ? message : "");
    return result_from_mut_doc(doc, true);
}

/* W01: union_session_open */
char *handle_union_session_open(cbm_mcp_server_t *srv, const char *args_json) {
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "arguments must be a JSON object");
    }

    yyjson_val *v_id = yyjson_obj_get(root, "identity");
    if (!v_id || !yyjson_is_str(v_id)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "identity is required");
    }
    const char *identity = yyjson_get_str(v_id);

    const char *contract_id = "";
    yyjson_val *v_contract = yyjson_obj_get(root, "contract_id");
    if (v_contract && yyjson_is_str(v_contract)) {
        contract_id = yyjson_get_str(v_contract);
    }

    const char *seq_str = "0";
    yyjson_val *v_seq = yyjson_obj_get(root, "based_on_seq");
    if (v_seq && yyjson_is_str(v_seq)) {
        seq_str = yyjson_get_str(v_seq);
    }

    uint32_t pid = 0;
    yyjson_val *v_pid = yyjson_obj_get(root, "client_pid");
    if (v_pid && yyjson_is_int(v_pid)) {
        pid = (uint32_t)yyjson_get_int(v_pid);
    } else {
#ifdef _WIN32
        pid = (uint32_t)GetCurrentProcessId();
#else
        pid = (uint32_t)getpid();
#endif
    }

    CbmSessionHorizon horizon;
    char err_buf[256] = {0};
    CbmSessionResult rc = cbm_session_open(get_sessions(srv), get_contracts(srv), get_pool(srv),
                                          pid, identity, contract_id, seq_str, &horizon,
                                          err_buf, sizeof(err_buf));
    yyjson_doc_free(doc);

    if (rc != CBM_SESSION_OK) {
        return json_error_result("SESSION_OPEN_FAILED", err_buf[0] ? err_buf : "failed to open session");
    }

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "success", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "horizon_id", horizon.horizon_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "identity", horizon.identity);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "contract_id", horizon.contract_id);
    yyjson_mut_obj_add_bool(out_doc, out_root, "restricted", horizon.restricted);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "open_refusal", cbm_refusal_code_string(horizon.open_refusal));
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "based_on_seq", horizon.based_on_seq);
    yyjson_mut_obj_add_uint(out_doc, out_root, "opened_at_unix", (uint64_t)horizon.opened_at_unix);
    return result_from_mut_doc(out_doc, false);
}

/* W01: union_session_get */
char *handle_union_session_get(cbm_mcp_server_t *srv, const char *args_json) {
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *v_id = root ? yyjson_obj_get(root, "horizon_id") : NULL;
    if (!v_id || !yyjson_is_str(v_id)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "horizon_id is required");
    }
    const char *horizon_id = yyjson_get_str(v_id);

    const CbmSessionHorizon *sh = cbm_session_get(get_sessions(srv), horizon_id);
    if (!sh) {
        yyjson_doc_free(doc);
        return json_error_result("SESSION_NOT_FOUND", "session not found for horizon_id");
    }

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "success", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "horizon_id", sh->horizon_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "identity", sh->identity);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "contract_id", sh->contract_id);
    yyjson_mut_obj_add_bool(out_doc, out_root, "restricted", sh->restricted);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "open_refusal", cbm_refusal_code_string(sh->open_refusal));
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "based_on_seq", sh->based_on_seq);
    yyjson_mut_obj_add_uint(out_doc, out_root, "opened_at_unix", (uint64_t)sh->opened_at_unix);
    yyjson_mut_obj_add_uint(out_doc, out_root, "actions", sh->actions);
    yyjson_mut_obj_add_uint(out_doc, out_root, "refusals", sh->refusals);
    yyjson_doc_free(doc);
    return result_from_mut_doc(out_doc, false);
}

/* W01: union_session_close */
char *handle_union_session_close(cbm_mcp_server_t *srv, const char *args_json) {
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *v_id = root ? yyjson_obj_get(root, "horizon_id") : NULL;
    if (!v_id || !yyjson_is_str(v_id)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "horizon_id is required");
    }
    const char *horizon_id = yyjson_get_str(v_id);

    CbmSessionCloseReason reason = CBM_SESSION_CLOSE_NORMAL;
    yyjson_val *v_reason = root ? yyjson_obj_get(root, "reason") : NULL;
    if (v_reason && yyjson_is_str(v_reason)) {
        const char *r_str = yyjson_get_str(v_reason);
        if (strcmp(r_str, "ABNORMAL") == 0) reason = CBM_SESSION_CLOSE_ABNORMAL;
        else if (strcmp(r_str, "EMPTY") == 0) reason = CBM_SESSION_CLOSE_EMPTY;
    }

    CbmSessionClosure closure;
    char err_buf[256] = {0};
    CbmSessionResult rc = cbm_session_close(get_sessions(srv), get_pool(srv), horizon_id, reason, &closure, err_buf, sizeof(err_buf));
    yyjson_doc_free(doc);

    if (rc != CBM_SESSION_OK) {
        return json_error_result("SESSION_NOT_FOUND", "failed to close session: not found");
    }

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "success", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "horizon_id", closure.horizon_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "identity", closure.identity);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "reason", closure.reason);
    yyjson_mut_obj_add_uint(out_doc, out_root, "duration_sec", (uint64_t)closure.duration_sec);
    yyjson_mut_obj_add_uint(out_doc, out_root, "actions", closure.actions);
    yyjson_mut_obj_add_uint(out_doc, out_root, "refusals", closure.refusals);
    return result_from_mut_doc(out_doc, false);
}

/* W02: union_record_action */
char *handle_union_record_action(cbm_mcp_server_t *srv, const char *args_json) {
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *v_hid = root ? yyjson_obj_get(root, "horizon_id") : NULL;
    yyjson_val *v_act = root ? yyjson_obj_get(root, "action_name") : NULL;
    if (!v_hid || !yyjson_is_str(v_hid) || !v_act || !yyjson_is_str(v_act)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "horizon_id and action_name are required");
    }
    const char *horizon_id = yyjson_get_str(v_hid);
    const char *action_name = yyjson_get_str(v_act);

    CbmEffectClass effect_class = CBM_EFFECT_IDEMPOTENT;
    yyjson_val *v_cls = root ? yyjson_obj_get(root, "effect_class") : NULL;
    if (v_cls && yyjson_is_str(v_cls)) {
        const char *cls_str = yyjson_get_str(v_cls);
        if (strcmp(cls_str, "COMPENSABLE") == 0) effect_class = CBM_EFFECT_COMPENSABLE;
        else if (strcmp(cls_str, "IRREVERSIBLE") == 0) effect_class = CBM_EFFECT_IRREVERSIBLE;
        else if (strcmp(cls_str, "UNCLASSIFIED") == 0) effect_class = CBM_EFFECT_UNCLASSIFIED;
    }

    const char *auth_id = NULL;
    yyjson_val *v_auth = root ? yyjson_obj_get(root, "auth_id") : NULL;
    if (v_auth && yyjson_is_str(v_auth)) auth_id = yyjson_get_str(v_auth);

    const char *idempotency_key = NULL;
    yyjson_val *v_key = root ? yyjson_obj_get(root, "idempotency_key") : NULL;
    if (v_key && yyjson_is_str(v_key)) idempotency_key = yyjson_get_str(v_key);

    CbmSessionRegistry *sessions = get_sessions(srv);
    const CbmSessionHorizon *sh = cbm_session_get(sessions, horizon_id);
    if (!sh) {
        yyjson_doc_free(doc);
        return json_error_result("SESSION_NOT_FOUND", "session not found");
    }

    /* G1/A01: Restricted mode blocks irreversible actions */
    if (sh->restricted && (effect_class == CBM_EFFECT_IRREVERSIBLE || effect_class == CBM_EFFECT_UNCLASSIFIED)) {
        cbm_session_record_refusal(sessions, horizon_id);
        cbm_refusal_emit(CBM_REFUSAL_CONTRACT_UNKNOWN, horizon_id,
                         "irreversible action blocked in restricted mode without registered contract");
        yyjson_doc_free(doc);
        return json_error_result("RESTRICTED_MODE_BLOCKED",
                                 "Irreversible action blocked in restricted mode without registered contract");
    }

    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    char reason[256] = {0};
    uint64_t now_unix = (uint64_t)time(NULL);
    CbmGatewayDecision dec = cbm_gateway_authorize_action(get_gateway(srv), horizon_id,
                                                         sh->identity, NULL, action_name,
                                                         effect_class, auth_id, sh->based_on_seq,
                                                         now_unix, idempotency_key,
                                                         &refusal, reason, sizeof(reason));
    if (dec == CBM_GATEWAY_BLOCK) {
        cbm_session_record_refusal(sessions, horizon_id);
        yyjson_doc_free(doc);
        return json_error_result(cbm_refusal_code_string(refusal),
                                 reason[0] ? reason : "action blocked by gateway");
    }

    cbm_session_record_action(sessions, horizon_id);
    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "authorized", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "horizon_id", horizon_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "action_name", action_name);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "effect_class", cbm_effect_class_string(effect_class));
    yyjson_doc_free(doc);
    return result_from_mut_doc(out_doc, false);
}

/* W04: classify_activity */
char *handle_classify_activity(cbm_mcp_server_t *srv, const char *args_json) {
    (void)srv;
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *v_intent = root ? yyjson_obj_get(root, "intent") : NULL;
    if (!v_intent || !yyjson_is_str(v_intent)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "intent is required");
    }
    const char *intent = yyjson_get_str(v_intent);

    CbmActivityClass cls = cbm_classify_activity(intent);
    const char *cls_str = (cls == CBM_ACTIVITY_SPECIALTY) ? "SPECIALTY" : "CONSULTATIVE";

    cbm_log(CBM_LOG_INFO, "union.routing moment=classify intent=%s verdict=%s", intent, cls_str);

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "success", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "activity_class", cls_str);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "intent", intent);
    yyjson_doc_free(doc);
    return result_from_mut_doc(out_doc, false);
}

/* W04: validate_provenance */
char *handle_validate_provenance(cbm_mcp_server_t *srv, const char *args_json) {
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "arguments must be an object");
    }

    CbmSpecialtyJudgment j;
    memset(&j, 0, sizeof(j));

    yyjson_val *v_tid = yyjson_obj_get(root, "theme_id");
    yyjson_val *v_dec = yyjson_obj_get(root, "declared_invention");

    if (v_tid && yyjson_is_str(v_tid) && yyjson_get_str(v_tid)[0] != '\0') {
        j.kind = CBM_PROVENANCE_CANON_CITATION;
        strncpy(j.citation.theme_id, yyjson_get_str(v_tid), sizeof(j.citation.theme_id) - 1);
        yyjson_val *v_uri = yyjson_obj_get(root, "node_uri");
        if (v_uri && yyjson_is_str(v_uri)) {
            strncpy(j.citation.node_uri, yyjson_get_str(v_uri), sizeof(j.citation.node_uri) - 1);
        }
        yyjson_val *v_ver = yyjson_obj_get(root, "pinned_version");
        if (v_ver && yyjson_is_str(v_ver)) {
            strncpy(j.citation.pinned_version, yyjson_get_str(v_ver), sizeof(j.citation.pinned_version) - 1);
        }
    } else if (v_dec && yyjson_is_bool(v_dec) && yyjson_get_bool(v_dec)) {
        j.kind = CBM_PROVENANCE_DECLARED_INVENTION;
        j.invention.declared = true;
        yyjson_val *v_rat = yyjson_obj_get(root, "rationale");
        if (v_rat && yyjson_is_str(v_rat)) {
            strncpy(j.invention.rationale, yyjson_get_str(v_rat), sizeof(j.invention.rationale) - 1);
        }
    } else {
        j.kind = CBM_PROVENANCE_NONE;
    }

    char err_buf[256] = {0};
    CbmRefusalCode refusal = cbm_validate_specialty_provenance(&j, get_themes(srv), err_buf, sizeof(err_buf));
    yyjson_doc_free(doc);

    if (refusal != CBM_REFUSAL_OK) {
        cbm_refusal_emit(refusal, "provenance", err_buf);
        return json_error_result(cbm_refusal_code_string(refusal),
                                 err_buf[0] ? err_buf : "provenance invalid");
    }

    const char *p_class = (j.kind == CBM_PROVENANCE_CANON_CITATION) ? "CANON_CITATION" : "DECLARED_INVENTION";
    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "valid", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "provenance_class", p_class);
    return result_from_mut_doc(out_doc, false);
}

/* W05: theme_lookup */
char *handle_theme_lookup(cbm_mcp_server_t *srv, const char *args_json) {
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *v_tid = root ? yyjson_obj_get(root, "theme_id") : NULL;
    if (!v_tid || !yyjson_is_str(v_tid)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "theme_id is required");
    }
    const char *theme_id = yyjson_get_str(v_tid);

    CbmThemeEntry entry;
    CbmRefusalCode rc = cbm_theme_registry_lookup(get_themes(srv), theme_id, &entry);
    yyjson_doc_free(doc);

    if (rc != CBM_REFUSAL_OK) {
        return json_error_result("THEME_UNKNOWN", "theme not found in registry");
    }

    const char *st_str = (entry.status == CBM_THEME_ABSENT) ? "ABSENT" : "ACTIVE";
    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "success", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "theme_id", entry.theme_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "namespace", entry.namespace);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "curator", entry.curator);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "version", entry.version);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "status", st_str);
    return result_from_mut_doc(out_doc, false);
}

/* W05: theme_register */
char *handle_theme_register(cbm_mcp_server_t *srv, const char *args_json) {
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "arguments must be an object");
    }

    CbmThemeEntry entry;
    memset(&entry, 0, sizeof(entry));

    yyjson_val *v_tid = yyjson_obj_get(root, "theme_id");
    yyjson_val *v_ns = yyjson_obj_get(root, "namespace");
    yyjson_val *v_cur = yyjson_obj_get(root, "curator");
    yyjson_val *v_ver = yyjson_obj_get(root, "version");

    if (v_tid && yyjson_is_str(v_tid)) strncpy(entry.theme_id, yyjson_get_str(v_tid), sizeof(entry.theme_id) - 1);
    if (v_ns && yyjson_is_str(v_ns)) strncpy(entry.namespace, yyjson_get_str(v_ns), sizeof(entry.namespace) - 1);
    if (v_cur && yyjson_is_str(v_cur)) strncpy(entry.curator, yyjson_get_str(v_cur), sizeof(entry.curator) - 1);
    if (v_ver && yyjson_is_str(v_ver)) strncpy(entry.version, yyjson_get_str(v_ver), sizeof(entry.version) - 1);

    entry.status = CBM_THEME_ACTIVE;
    yyjson_val *v_st = yyjson_obj_get(root, "status");
    if (v_st && yyjson_is_str(v_st) && strcmp(yyjson_get_str(v_st), "ABSENT") == 0) {
        entry.status = CBM_THEME_ABSENT;
    }

    char err_buf[256] = {0};
    CbmRefusalCode rc = cbm_theme_registry_register(get_themes(srv), &entry, err_buf, sizeof(err_buf));
    yyjson_doc_free(doc);

    if (rc != CBM_REFUSAL_OK) {
        return json_error_result(cbm_refusal_code_string(rc), err_buf[0] ? err_buf : "failed to register theme");
    }

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "success", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "theme_id", entry.theme_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "status", entry.status == CBM_THEME_ABSENT ? "ABSENT" : "ACTIVE");
    return result_from_mut_doc(out_doc, false);
}

/* W05: binding_claim */
char *handle_binding_claim(cbm_mcp_server_t *srv, const char *args_json) {
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "arguments must be an object");
    }

    CbmBindingClaim claim;
    memset(&claim, 0, sizeof(claim));

    yyjson_val *v_cid = yyjson_obj_get(root, "claim_id");
    yyjson_val *v_tid = yyjson_obj_get(root, "theme_id");
    yyjson_val *v_ver = yyjson_obj_get(root, "pinned_version");
    yyjson_val *v_val = yyjson_obj_get(root, "validated_by");
    yyjson_val *v_mod = yyjson_obj_get(root, "mode");
    yyjson_val *v_scp = yyjson_obj_get(root, "binding_scope");

    if (v_cid && yyjson_is_str(v_cid)) strncpy(claim.claim_id, yyjson_get_str(v_cid), sizeof(claim.claim_id) - 1);
    if (v_tid && yyjson_is_str(v_tid)) strncpy(claim.theme_id, yyjson_get_str(v_tid), sizeof(claim.theme_id) - 1);
    if (v_ver && yyjson_is_str(v_ver)) strncpy(claim.pinned_version, yyjson_get_str(v_ver), sizeof(claim.pinned_version) - 1);
    if (v_val && yyjson_is_str(v_val)) strncpy(claim.validated_by, yyjson_get_str(v_val), sizeof(claim.validated_by) - 1);
    if (v_scp && yyjson_is_str(v_scp)) strncpy(claim.binding_scope, yyjson_get_str(v_scp), sizeof(claim.binding_scope) - 1);

    claim.mode = CBM_BINDING_NORMATIVE;
    if (v_mod && yyjson_is_str(v_mod) && strcmp(yyjson_get_str(v_mod), "CONSULTED") == 0) {
        claim.mode = CBM_BINDING_CONSULTED;
    }

    char err_buf[256] = {0};
    CbmRefusalCode rc = cbm_binding_validate_and_admit(get_bindings(srv), &claim, err_buf, sizeof(err_buf));
    yyjson_doc_free(doc);

    if (rc != CBM_REFUSAL_OK) {
        cbm_refusal_emit(rc, "binding_claim", err_buf);
        return json_error_result(cbm_refusal_code_string(rc), err_buf[0] ? err_buf : "binding validation failed");
    }

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "admitted", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "claim_id", claim.claim_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "theme_id", claim.theme_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "mode", claim.mode == CBM_BINDING_NORMATIVE ? "NORMATIVE" : "CONSULTED");
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "status", "ACTIVE");
    return result_from_mut_doc(out_doc, false);
}

/* W05: founding_propose */
char *handle_founding_propose(cbm_mcp_server_t *srv, const char *args_json) {
    (void)srv;
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *v_tid = root ? yyjson_obj_get(root, "suggested_theme_id") : NULL;
    yyjson_val *v_ns = root ? yyjson_obj_get(root, "namespace") : NULL;
    yyjson_val *v_rat = root ? yyjson_obj_get(root, "rationale") : NULL;
    yyjson_val *v_sess = root ? yyjson_obj_get(root, "origin_session") : NULL;

    if (!v_tid || !yyjson_is_str(v_tid) || !v_ns || !yyjson_is_str(v_ns) ||
        !v_rat || !yyjson_is_str(v_rat) || !v_sess || !yyjson_is_str(v_sess)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "suggested_theme_id, namespace, rationale, and origin_session are required");
    }

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "proposed", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "suggested_theme_id", yyjson_get_str(v_tid));
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "namespace", yyjson_get_str(v_ns));
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "origin_session", yyjson_get_str(v_sess));
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "status", "WAITING_HUMAN");
    yyjson_doc_free(doc);
    return result_from_mut_doc(out_doc, false);
}

/* W05: founding_decide */
char *handle_founding_decide(cbm_mcp_server_t *srv, const char *args_json) {
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *v_tid = root ? yyjson_obj_get(root, "suggested_theme_id") : NULL;
    yyjson_val *v_acc = root ? yyjson_obj_get(root, "operator_accepted") : NULL;
    yyjson_val *v_auto = root ? yyjson_obj_get(root, "is_agent_autonomous") : NULL;

    if (!v_tid || !yyjson_is_str(v_tid) || !v_acc || !yyjson_is_bool(v_acc)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "suggested_theme_id and operator_accepted (bool) are required");
    }

    CbmFoundingProposal prop;
    memset(&prop, 0, sizeof(prop));
    strncpy(prop.suggested_theme_id, yyjson_get_str(v_tid), sizeof(prop.suggested_theme_id) - 1);
    strncpy(prop.namespace, "craft", sizeof(prop.namespace) - 1);
    strncpy(prop.suggested_curator, "operator", sizeof(prop.suggested_curator) - 1);

    bool accepted = yyjson_get_bool(v_acc);
    bool autonomous = (v_auto && yyjson_is_bool(v_auto)) ? yyjson_get_bool(v_auto) : false;

    CbmClosureExclusions excl;
    memset(&excl, 0, sizeof(excl));

    CbmRefusalCode rc = cbm_founding_adjudicate(get_themes(srv), &prop, accepted, autonomous, &excl);
    yyjson_doc_free(doc);

    if (rc != CBM_REFUSAL_OK) {
        return json_error_result(cbm_refusal_code_string(rc), "founding decision rejected or declined");
    }

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "decided", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "suggested_theme_id", prop.suggested_theme_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "status", "DRAFT");
    return result_from_mut_doc(out_doc, false);
}

/* W07: contest_verify */
char *handle_contest_verify(cbm_mcp_server_t *srv, const char *args_json) {
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *v_tgt = root ? yyjson_obj_get(root, "target_ref") : NULL;
    yyjson_val *v_ev = root ? yyjson_obj_get(root, "evidence") : NULL;

    if (!v_tgt || !yyjson_is_str(v_tgt) || !v_ev || !yyjson_is_arr(v_ev) || yyjson_arr_size(v_ev) == 0) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "target_ref and non-empty evidence array are required");
    }

    CbmContestation contest;
    memset(&contest, 0, sizeof(contest));
    snprintf(contest.contest_id, sizeof(contest.contest_id), "contest_%llu_%llu",
             (unsigned long long)time(NULL), (unsigned long long)(++s_contest_seq));
    strncpy(contest.target_ref, yyjson_get_str(v_tgt), sizeof(contest.target_ref) - 1);
    strncpy(contest.submitter_identity, "blind_reviser", sizeof(contest.submitter_identity) - 1);

    contest.severity = CBM_CONTEST_BLOCKING;
    yyjson_val *v_sev = yyjson_obj_get(root, "severity");
    if (v_sev && yyjson_is_str(v_sev)) {
        const char *sev_str = yyjson_get_str(v_sev);
        if (strcmp(sev_str, "INFORMATIVE") == 0) contest.severity = CBM_CONTEST_INFORMATIVE;
        else if (strcmp(sev_str, "INVALIDATING") == 0) contest.severity = CBM_CONTEST_INVALIDATING;
    }

    size_t idx, max;
    yyjson_val *item;
    yyjson_arr_foreach(v_ev, idx, max, item) {
        if (contest.evidence_count < CBM_CONTEST_EVIDENCE_CAP && yyjson_is_str(item)) {
            strncpy(contest.evidence[contest.evidence_count++], yyjson_get_str(item), CBM_CONTEST_EVIDENCE_LEN - 1);
        }
    }

    char err_buf[256] = {0};
    CbmRefusalCode rc = cbm_contest_submit(get_contests(srv), &contest, err_buf, sizeof(err_buf));
    yyjson_doc_free(doc);

    if (rc != CBM_REFUSAL_OK) {
        return json_error_result(cbm_refusal_code_string(rc),
                                 err_buf[0] ? err_buf : "failed to submit contest");
    }

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "contested", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "contest_id", contest.contest_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "target_ref", contest.target_ref);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "severity", cbm_contest_severity_string(contest.severity));
    return result_from_mut_doc(out_doc, false);
}
