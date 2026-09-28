#include "union_handler.h"
#include "mcp.h"
#include "mcp_internal.h"
#include "../foundation/log.h"
#include "../foundation/platform.h"
#include "../foundation/compat.h"
#include "../foundation/compat_fs.h"
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

#define MAX_STORED_PROPOSALS 64
typedef struct {
    bool active;
    CbmFoundingProposal proposal;
} CbmStoredProposal;

static CbmStoredProposal s_proposals[MAX_STORED_PROPOSALS];
static bool s_proposals_loaded = false;

static void get_proposals_file_path(char *buf, size_t sz) {
    const char *dir = cbm_resolve_cache_dir();
    if (!dir) dir = cbm_tmpdir();
    snprintf(buf, sz, "%s/founding_proposals.json", dir);
}

static bool save_proposals_to_disk(void) {
    char path[1024];
    get_proposals_file_path(path, sizeof(path));
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) return false;
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, arr);

    for (size_t i = 0; i < MAX_STORED_PROPOSALS; i++) {
        if (s_proposals[i].active) {
            yyjson_mut_val *obj = yyjson_mut_obj(doc);
            yyjson_mut_obj_add_strcpy(doc, obj, "suggested_theme_id", s_proposals[i].proposal.suggested_theme_id);
            yyjson_mut_obj_add_strcpy(doc, obj, "namespace", s_proposals[i].proposal.namespace);
            yyjson_mut_obj_add_strcpy(doc, obj, "rationale", s_proposals[i].proposal.rationale);
            yyjson_mut_obj_add_strcpy(doc, obj, "origin_session", s_proposals[i].proposal.origin_session);
            yyjson_mut_obj_add_strcpy(doc, obj, "suggested_curator", s_proposals[i].proposal.suggested_curator);
            yyjson_mut_arr_add_val(arr, obj);
        }
    }

    bool ok = yyjson_mut_write_file(path, doc, 0, NULL, NULL);
    yyjson_mut_doc_free(doc);
    return ok;
}

static void load_proposals_from_disk(void) {
    if (s_proposals_loaded) return;
    s_proposals_loaded = true;

    char path[1024];
    get_proposals_file_path(path, sizeof(path));
    if (!cbm_file_exists(path)) return;

    yyjson_doc *doc = yyjson_read_file(path, 0, NULL, NULL);
    if (!doc) return;

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (root && yyjson_is_arr(root)) {
        size_t idx, max;
        yyjson_val *val;
        yyjson_arr_foreach(root, idx, max, val) {
            if (idx >= MAX_STORED_PROPOSALS) break;
            yyjson_val *v_th = yyjson_obj_get(val, "suggested_theme_id");
            if (v_th && yyjson_is_str(v_th)) {
                s_proposals[idx].active = true;
                strncpy(s_proposals[idx].proposal.suggested_theme_id, yyjson_get_str(v_th), sizeof(s_proposals[idx].proposal.suggested_theme_id) - 1);
                yyjson_val *v_ns = yyjson_obj_get(val, "namespace");
                if (v_ns && yyjson_is_str(v_ns)) {
                    strncpy(s_proposals[idx].proposal.namespace, yyjson_get_str(v_ns), sizeof(s_proposals[idx].proposal.namespace) - 1);
                }
                yyjson_val *v_rat = yyjson_obj_get(val, "rationale");
                if (v_rat && yyjson_is_str(v_rat)) {
                    strncpy(s_proposals[idx].proposal.rationale, yyjson_get_str(v_rat), sizeof(s_proposals[idx].proposal.rationale) - 1);
                }
                yyjson_val *v_orig = yyjson_obj_get(val, "origin_session");
                if (v_orig && yyjson_is_str(v_orig)) {
                    strncpy(s_proposals[idx].proposal.origin_session, yyjson_get_str(v_orig), sizeof(s_proposals[idx].proposal.origin_session) - 1);
                }
                yyjson_val *v_cur = yyjson_obj_get(val, "suggested_curator");
                if (v_cur && yyjson_is_str(v_cur)) {
                    strncpy(s_proposals[idx].proposal.suggested_curator, yyjson_get_str(v_cur), sizeof(s_proposals[idx].proposal.suggested_curator) - 1);
                }
            }
        }
    }
    yyjson_doc_free(doc);
}

void cbm_union_reset_proposal_store_for_test(void) {
    for (size_t i = 0; i < MAX_STORED_PROPOSALS; i++) {
        s_proposals[i].active = false;
        memset(&s_proposals[i], 0, sizeof(s_proposals[i]));
    }
    s_proposals_loaded = true;
    char path[1024];
    get_proposals_file_path(path, sizeof(path));
    if (cbm_file_exists(path)) {
        cbm_unlink(path);
    }
}

void cbm_union_reload_proposals_for_test(void) {
    for (size_t i = 0; i < MAX_STORED_PROPOSALS; i++) {
        s_proposals[i].active = false;
        memset(&s_proposals[i], 0, sizeof(s_proposals[i]));
    }
    s_proposals_loaded = false;
    load_proposals_from_disk();
}

typedef enum {
    CBM_STORE_PROP_OK = 0,
    CBM_STORE_PROP_FULL,
    CBM_STORE_PROP_PERSISTENCE_FAILED,
    CBM_STORE_PROP_INVALID
} CbmStoreProposalResult;

static CbmStoreProposalResult store_proposal(const char *theme_id, const char *namespace, const char *rationale,
                                            const char *origin_session, const char *curator) {
    load_proposals_from_disk();
    if (!theme_id || !theme_id[0]) return CBM_STORE_PROP_INVALID;
    int slot = -1;
    for (size_t i = 0; i < MAX_STORED_PROPOSALS; i++) {
        if (s_proposals[i].active && strcmp(s_proposals[i].proposal.suggested_theme_id, theme_id) == 0) {
            slot = (int)i;
            break;
        }
        if (!s_proposals[i].active && slot == -1) {
            slot = (int)i;
        }
    }
    if (slot == -1) {
        return CBM_STORE_PROP_FULL;
    }

    CbmStoredProposal backup;
    memcpy(&backup, &s_proposals[slot], sizeof(backup));

    s_proposals[slot].active = true;
    memset(&s_proposals[slot].proposal, 0, sizeof(CbmFoundingProposal));
    strncpy(s_proposals[slot].proposal.suggested_theme_id, theme_id, sizeof(s_proposals[slot].proposal.suggested_theme_id) - 1);
    if (namespace) strncpy(s_proposals[slot].proposal.namespace, namespace, sizeof(s_proposals[slot].proposal.namespace) - 1);
    if (rationale) strncpy(s_proposals[slot].proposal.rationale, rationale, sizeof(s_proposals[slot].proposal.rationale) - 1);
    if (origin_session) strncpy(s_proposals[slot].proposal.origin_session, origin_session, sizeof(s_proposals[slot].proposal.origin_session) - 1);
    if (curator) strncpy(s_proposals[slot].proposal.suggested_curator, curator, sizeof(s_proposals[slot].proposal.suggested_curator) - 1);

    if (!save_proposals_to_disk()) {
        memcpy(&s_proposals[slot], &backup, sizeof(backup));
        return CBM_STORE_PROP_PERSISTENCE_FAILED;
    }
    return CBM_STORE_PROP_OK;
}

static CbmFoundingProposal *find_proposal(const char *theme_id) {
    load_proposals_from_disk();
    if (!theme_id || !theme_id[0]) return NULL;
    for (size_t i = 0; i < MAX_STORED_PROPOSALS; i++) {
        if (s_proposals[i].active && strcmp(s_proposals[i].proposal.suggested_theme_id, theme_id) == 0) {
            return &s_proposals[i].proposal;
        }
    }
    return NULL;
}

static void remove_proposal(const char *theme_id) {
    load_proposals_from_disk();
    if (!theme_id || !theme_id[0]) return;
    for (size_t i = 0; i < MAX_STORED_PROPOSALS; i++) {
        if (s_proposals[i].active && strcmp(s_proposals[i].proposal.suggested_theme_id, theme_id) == 0) {
            s_proposals[i].active = false;
            memset(&s_proposals[i], 0, sizeof(s_proposals[i]));
            save_proposals_to_disk();
            return;
        }
    }
}

static CbmSessionSweepContext s_sweep_contexts[CBM_SESSION_REGISTRY_CAP];
static bool s_sweep_contexts_used[CBM_SESSION_REGISTRY_CAP];

CbmSessionSweepContext *cbm_mcp_get_session_sweep_context(const char *horizon_id) {
    if (!horizon_id || !horizon_id[0]) return NULL;
    for (size_t i = 0; i < CBM_SESSION_REGISTRY_CAP; i++) {
        if (s_sweep_contexts_used[i] && strcmp(s_sweep_contexts[i].horizon_id, horizon_id) == 0) {
            return &s_sweep_contexts[i];
        }
    }
    return NULL;
}

static CbmSessionSweepContext *cbm_mcp_allocate_session_sweep_context(const char *horizon_id) {
    if (!horizon_id || !horizon_id[0]) return NULL;
    for (size_t i = 0; i < CBM_SESSION_REGISTRY_CAP; i++) {
        if (s_sweep_contexts_used[i] && strcmp(s_sweep_contexts[i].horizon_id, horizon_id) == 0) {
            return &s_sweep_contexts[i];
        }
    }
    for (size_t i = 0; i < CBM_SESSION_REGISTRY_CAP; i++) {
        if (!s_sweep_contexts_used[i]) {
            s_sweep_contexts_used[i] = true;
            cbm_sweep_init(&s_sweep_contexts[i], horizon_id, horizon_id);
            return &s_sweep_contexts[i];
        }
    }
    return NULL;
}

static void release_sweep_context(const char *horizon_id) {
    if (!horizon_id || !horizon_id[0]) return;
    for (size_t i = 0; i < CBM_SESSION_REGISTRY_CAP; i++) {
        if (s_sweep_contexts_used[i] && strcmp(s_sweep_contexts[i].horizon_id, horizon_id) == 0) {
            s_sweep_contexts_used[i] = false;
            memset(&s_sweep_contexts[i], 0, sizeof(s_sweep_contexts[i]));
            return;
        }
    }
}

static const char *find_horizon_for_claim(const char *claim_id) {
    if (!claim_id || !claim_id[0]) return NULL;
    for (size_t i = 0; i < CBM_SESSION_REGISTRY_CAP; i++) {
        if (s_sweep_contexts_used[i]) {
            if (cbm_sweep_find_claim(&s_sweep_contexts[i], claim_id) != NULL) {
                return s_sweep_contexts[i].horizon_id;
            }
        }
    }
    return NULL;
}

typedef struct {
    char horizon_id[CBM_HORIZON_ID_MAX];
    uint32_t idempotent;
    uint32_t compensable;
    uint32_t irreversible;
} CbmSessionActionLedger;

static CbmSessionActionLedger s_action_ledgers[CBM_SESSION_REGISTRY_CAP];

static CbmSessionActionLedger *get_or_create_action_ledger(const char *horizon_id) {
    if (!horizon_id || !horizon_id[0]) return NULL;
    for (size_t i = 0; i < CBM_SESSION_REGISTRY_CAP; i++) {
        if (s_action_ledgers[i].horizon_id[0] && strcmp(s_action_ledgers[i].horizon_id, horizon_id) == 0) {
            return &s_action_ledgers[i];
        }
    }
    for (size_t i = 0; i < CBM_SESSION_REGISTRY_CAP; i++) {
        if (!s_action_ledgers[i].horizon_id[0]) {
            strncpy(s_action_ledgers[i].horizon_id, horizon_id, sizeof(s_action_ledgers[i].horizon_id) - 1);
            s_action_ledgers[i].idempotent = 0;
            s_action_ledgers[i].compensable = 0;
            s_action_ledgers[i].irreversible = 0;
            return &s_action_ledgers[i];
        }
    }
    return NULL;
}

static CbmSessionActionLedger *find_action_ledger(const char *horizon_id) {
    if (!horizon_id || !horizon_id[0]) return NULL;
    for (size_t i = 0; i < CBM_SESSION_REGISTRY_CAP; i++) {
        if (s_action_ledgers[i].horizon_id[0] && strcmp(s_action_ledgers[i].horizon_id, horizon_id) == 0) {
            return &s_action_ledgers[i];
        }
    }
    return NULL;
}

static void release_action_ledger(const char *horizon_id) {
    if (!horizon_id || !horizon_id[0]) return;
    for (size_t i = 0; i < CBM_SESSION_REGISTRY_CAP; i++) {
        if (s_action_ledgers[i].horizon_id[0] && strcmp(s_action_ledgers[i].horizon_id, horizon_id) == 0) {
            memset(&s_action_ledgers[i], 0, sizeof(s_action_ledgers[i]));
            return;
        }
    }
}

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

    cbm_mcp_allocate_session_sweep_context(horizon.horizon_id);

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
    char horizon_id[CBM_HORIZON_ID_MAX] = {0};
    strncpy(horizon_id, yyjson_get_str(v_id), sizeof(horizon_id) - 1);

    CbmSessionCloseReason reason = CBM_SESSION_CLOSE_NORMAL;
    yyjson_val *v_reason = root ? yyjson_obj_get(root, "reason") : NULL;
    if (v_reason && yyjson_is_str(v_reason)) {
        const char *r_str = yyjson_get_str(v_reason);
        if (strcmp(r_str, "ABNORMAL") == 0) reason = CBM_SESSION_CLOSE_ABNORMAL;
        else if (strcmp(r_str, "EMPTY") == 0) reason = CBM_SESSION_CLOSE_EMPTY;
    }

    CbmSessionRegistry *sessions = get_sessions(srv);
    const CbmSessionHorizon *sh = cbm_session_get(sessions, horizon_id);
    if (!sh) {
        yyjson_doc_free(doc);
        return json_error_result("SESSION_NOT_FOUND", "failed to close session: not found");
    }

    /* Check sweep context: if claims exist, perform sweep */
    CbmSessionSweepContext *sweep_ctx = cbm_mcp_get_session_sweep_context(horizon_id);
    if (sweep_ctx && sweep_ctx->claim_count > 0) {
        char unresolved_list[512] = {0};
        char sweep_reason[256] = {0};
        CbmRefusalCode sweep_rc = cbm_sweep_close_session(sweep_ctx, NULL, unresolved_list, sizeof(unresolved_list),
                                                         sweep_reason, sizeof(sweep_reason));
        if (sweep_rc != CBM_REFUSAL_OK) {
            cbm_refusal_emit(sweep_rc, horizon_id, sweep_reason[0] ? sweep_reason : "sweep incomplete: unresolved claims remain");
            char err_msg[1024];
            snprintf(err_msg, sizeof(err_msg), "Session closure blocked: sweep incomplete. Unresolved claims: %s", unresolved_list);
            yyjson_doc_free(doc);
            return json_error_result(cbm_refusal_code_string(sweep_rc), err_msg);
        }
    }

    /* Build session trace before destroying session horizon */
    CbmSessionTrace trace;
    cbm_trace_init(&trace, sh->horizon_id, sh->identity, sh->contract_id);
    snprintf(trace.based_on_seq, sizeof(trace.based_on_seq), "%s", sh->based_on_seq);
    snprintf(trace.outcome, sizeof(trace.outcome), "%s", cbm_session_close_reason_string(reason));
    CbmSessionActionLedger *al = find_action_ledger(horizon_id);
    if (al) {
        trace.actions_idempotent = al->idempotent;
        trace.actions_compensable = al->compensable;
        trace.actions_irreversible = al->irreversible;
        trace.actions_total = al->idempotent + al->compensable + al->irreversible;
    } else {
        trace.actions_total = sh->actions;
        trace.actions_idempotent = sh->actions;
    }
    trace.refusal_count = sh->refusals;
    if (sweep_ctx) {
        trace.exclusions.declared = (sweep_ctx->report.discarded_total_count > 0);
        trace.exclusions.counts[CBM_EXCLUSION_OUT_OF_SCOPE] = (uint32_t)sweep_ctx->report.discarded_total_count;
    }

    CbmSessionClosure closure;
    char err_buf[256] = {0};
    CbmSessionResult rc = cbm_session_close(sessions, get_pool(srv), horizon_id, reason, &closure, err_buf, sizeof(err_buf));
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

    char trace_buf[1024] = {0};
    if (cbm_trace_format_evaluator_json(&trace, trace_buf, sizeof(trace_buf)) == 0) {
        cbm_log(CBM_LOG_INFO, "union.session.trace",
                "horizon_id", closure.horizon_id,
                "identity", closure.identity,
                "reason", closure.reason,
                "trace", trace_buf, NULL);

        yyjson_doc *t_doc = yyjson_read(trace_buf, strlen(trace_buf), 0);
        if (t_doc) {
            yyjson_mut_val *t_val = yyjson_val_mut_copy(out_doc, yyjson_doc_get_root(t_doc));
            if (t_val) {
                yyjson_mut_obj_add_val(out_doc, out_root, "trace", t_val);
            }
            yyjson_doc_free(t_doc);
        }
    }

    release_sweep_context(closure.horizon_id);
    release_action_ledger(closure.horizon_id);
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
    CbmSessionActionLedger *al = get_or_create_action_ledger(horizon_id);
    if (al) {
        if (effect_class == CBM_EFFECT_COMPENSABLE) al->compensable++;
        else if (effect_class == CBM_EFFECT_IRREVERSIBLE) al->irreversible++;
        else al->idempotent++;
    }
    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "authorized", true);
    yyjson_mut_obj_add_bool(out_doc, out_root, "recorded", true);
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
    yyjson_val *v_cur = root ? yyjson_obj_get(root, "suggested_curator") : NULL;

    if (!v_tid || !yyjson_is_str(v_tid) || !v_ns || !yyjson_is_str(v_ns) ||
        !v_rat || !yyjson_is_str(v_rat) || !v_sess || !yyjson_is_str(v_sess)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "suggested_theme_id, namespace, rationale, and origin_session are required");
    }

    const char *theme_id = yyjson_get_str(v_tid);
    CbmStoreProposalResult sres = store_proposal(theme_id, yyjson_get_str(v_ns), yyjson_get_str(v_rat),
                                                 yyjson_get_str(v_sess), (v_cur && yyjson_is_str(v_cur)) ? yyjson_get_str(v_cur) : "operator");
    if (sres == CBM_STORE_PROP_FULL) {
        yyjson_doc_free(doc);
        cbm_refusal_emit(CBM_REFUSAL_BUDGET_EXHAUSTED, "founding_propose",
                         "proposal capacity exhausted; cannot store proposal");
        return json_error_result("PROPOSAL_STORE_FULL",
                                 "Proposal store capacity exhausted; cannot accept more proposals");
    }
    if (sres == CBM_STORE_PROP_PERSISTENCE_FAILED) {
        yyjson_doc_free(doc);
        cbm_refusal_emit(CBM_REFUSAL_BUDGET_EXHAUSTED, "founding_propose",
                         "failed to persist founding proposal to disk");
        return json_error_result("PERSISTENCE_FAILED",
                                 "Failed to persist founding proposal to disk; proposal was not stored");
    }
    if (sres != CBM_STORE_PROP_OK) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "failed to store proposal");
    }

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "proposed", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "suggested_theme_id", theme_id);
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

    char theme_id[CBM_THEME_ID_MAX] = {0};
    strncpy(theme_id, yyjson_get_str(v_tid), sizeof(theme_id) - 1);
    CbmFoundingProposal *stored = find_proposal(theme_id);
    if (!stored) {
        yyjson_doc_free(doc);
        return json_error_result("PROPOSAL_NOT_FOUND", "No active founding proposal found for suggested_theme_id");
    }

    /* Verify operator authority: must be proven from authenticated host */
    yyjson_val *v_dec_by = root ? yyjson_obj_get(root, "decided_by") : NULL;
    if (!v_dec_by) v_dec_by = root ? yyjson_obj_get(root, "operator_identity") : NULL;
    const char *decided_by = (v_dec_by && yyjson_is_str(v_dec_by)) ? yyjson_get_str(v_dec_by) : NULL;

    yyjson_val *v_tok = root ? yyjson_obj_get(root, "operator_token") : NULL;
    if (!v_tok) v_tok = root ? yyjson_obj_get(root, "auth_token") : NULL;
    const char *auth_token = (v_tok && yyjson_is_str(v_tok)) ? yyjson_get_str(v_tok) : NULL;

    bool autonomous = (v_auto && yyjson_is_bool(v_auto)) ? yyjson_get_bool(v_auto) : false;
    if (autonomous || !cbm_mcp_server_verify_operator_authority(srv, decided_by, auth_token)) {
        yyjson_doc_free(doc);
        cbm_refusal_emit(CBM_REFUSAL_AUTO_FOUNDING_FORBIDDEN, "founding_decide",
                         "founding decisions require proven host operator authority; client self-declaration or autonomous adjudication is forbidden");
        return json_error_result("AUTO_FOUNDING_FORBIDDEN",
                                 "Founding decisions require proven host operator authority; client self-declaration or autonomous adjudication is forbidden");
    }

    CbmFoundingProposal prop;
    memcpy(&prop, stored, sizeof(prop));
    const char *proven_op = cbm_mcp_server_get_operator_identity(srv);
    if (proven_op && proven_op[0]) {
        strncpy(prop.suggested_curator, proven_op, sizeof(prop.suggested_curator) - 1);
    } else if (decided_by && decided_by[0]) {
        strncpy(prop.suggested_curator, decided_by, sizeof(prop.suggested_curator) - 1);
    }
    bool accepted = yyjson_get_bool(v_acc);

    CbmClosureExclusions excl;
    memset(&excl, 0, sizeof(excl));

    CbmRefusalCode rc = cbm_founding_adjudicate(get_themes(srv), &prop, accepted, false, &excl);
    yyjson_doc_free(doc);

    /* Consume proposal so it cannot be re-adjudicated */
    remove_proposal(theme_id);

    if (rc != CBM_REFUSAL_OK) {
        return json_error_result(cbm_refusal_code_string(rc), "founding decision rejected or declined");
    }

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "decided", true);
    yyjson_mut_obj_add_bool(out_doc, out_root, "accepted", accepted);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "suggested_theme_id", prop.suggested_theme_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "status", accepted ? "DRAFT" : "DECLINED");
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

    yyjson_val *v_th = root ? yyjson_obj_get(root, "target_horizon") : NULL;
    if (v_th && yyjson_is_str(v_th)) {
        strncpy(contest.target_horizon, yyjson_get_str(v_th), sizeof(contest.target_horizon) - 1);
    } else {
        const char *h_id = find_horizon_for_claim(contest.target_ref);
        if (h_id) {
            strncpy(contest.target_horizon, h_id, sizeof(contest.target_horizon) - 1);
        }
    }

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
    if (contest.target_horizon[0]) {
        yyjson_mut_obj_add_strcpy(out_doc, out_root, "target_horizon", contest.target_horizon);
    }
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "severity", cbm_contest_severity_string(contest.severity));
    return result_from_mut_doc(out_doc, false);
}

/* W01 / Scope C03: union_claim_capture */
char *handle_union_claim_capture(cbm_mcp_server_t *srv, const char *args_json) {
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *v_hid = root ? yyjson_obj_get(root, "horizon_id") : NULL;
    yyjson_val *v_cid = root ? yyjson_obj_get(root, "claim_id") : NULL;
    yyjson_val *v_type = root ? yyjson_obj_get(root, "type") : NULL;
    yyjson_val *v_pred = root ? yyjson_obj_get(root, "predicate") : NULL;

    if (!v_hid || !yyjson_is_str(v_hid) ||
        !v_cid || !yyjson_is_str(v_cid) ||
        !v_type || !yyjson_is_str(v_type) ||
        !v_pred || !yyjson_is_str(v_pred)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "horizon_id, claim_id, type, and predicate are required");
    }

    char horizon_id[CBM_HORIZON_ID_MAX] = {0};
    strncpy(horizon_id, yyjson_get_str(v_hid), sizeof(horizon_id) - 1);

    CbmSessionRegistry *sessions = get_sessions(srv);
    const CbmSessionHorizon *sh = cbm_session_get(sessions, horizon_id);
    if (!sh) {
        yyjson_doc_free(doc);
        cbm_refusal_emit(CBM_REFUSAL_CONTRACT_UNKNOWN, "union_claim_capture",
                         "claims cannot be captured outside an active open session");
        return json_error_result("SESSION_NOT_FOUND",
                                 "No active open session found for horizon_id; claims must be bound to a session");
    }

    CbmSessionSweepContext *sweep_ctx = cbm_mcp_get_session_sweep_context(horizon_id);
    if (!sweep_ctx) {
        yyjson_doc_free(doc);
        return json_error_result("SESSION_NOT_FOUND", "no sweep context found for active session");
    }

    CbmClaim claim;
    memset(&claim, 0, sizeof(claim));
    strncpy(claim.id, yyjson_get_str(v_cid), sizeof(claim.id) - 1);
    strncpy(claim.predicate, yyjson_get_str(v_pred), sizeof(claim.predicate) - 1);
    claim.type = cbm_claim_type_from_string(yyjson_get_str(v_type));

    yyjson_val *v_cons = yyjson_obj_get(root, "consequence");
    if (v_cons && yyjson_is_str(v_cons)) {
        strncpy(claim.consequence, yyjson_get_str(v_cons), sizeof(claim.consequence) - 1);
    }
    yyjson_val *v_seq = yyjson_obj_get(root, "based_on_seq");
    if (v_seq && yyjson_is_str(v_seq)) {
        strncpy(claim.based_on_seq, yyjson_get_str(v_seq), sizeof(claim.based_on_seq) - 1);
    } else if (sh->based_on_seq[0]) {
        strncpy(claim.based_on_seq, sh->based_on_seq, sizeof(claim.based_on_seq) - 1);
    } else {
        strncpy(claim.based_on_seq, "0", sizeof(claim.based_on_seq) - 1);
    }
    uint64_t exchange_seq = 1;
    yyjson_val *v_eseq = yyjson_obj_get(root, "exchange_seq");
    if (v_eseq && yyjson_is_int(v_eseq)) {
        exchange_seq = (uint64_t)yyjson_get_uint(v_eseq);
    }

    char reason[256] = {0};
    CbmRefusalCode rc = cbm_sweep_capture_claim(sweep_ctx, &claim, exchange_seq, reason, sizeof(reason));
    yyjson_doc_free(doc);

    if (rc != CBM_REFUSAL_OK) {
        return json_error_result(cbm_refusal_code_string(rc), reason[0] ? reason : "failed to capture claim");
    }

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "captured", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "horizon_id", horizon_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "claim_id", claim.id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "type", cbm_claim_type_string(claim.type));
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "status", "PROPOSED");
    return result_from_mut_doc(out_doc, false);
}

/* W01 / Scope C03: union_claim_resolve */
char *handle_union_claim_resolve(cbm_mcp_server_t *srv, const char *args_json) {
    if (!args_json) {
        return json_error_result("INVALID_PARAMS", "missing arguments");
    }
    yyjson_doc *doc = yyjson_read(args_json, strlen(args_json), 0);
    if (!doc) {
        return json_error_result("INVALID_PARAMS", "malformed JSON arguments");
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *v_hid = root ? yyjson_obj_get(root, "horizon_id") : NULL;
    yyjson_val *v_cid = root ? yyjson_obj_get(root, "claim_id") : NULL;
    yyjson_val *v_dest = root ? yyjson_obj_get(root, "destination") : NULL;

    if (!v_hid || !yyjson_is_str(v_hid) ||
        !v_cid || !yyjson_is_str(v_cid) ||
        !v_dest || !yyjson_is_str(v_dest)) {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "horizon_id, claim_id, and destination are required");
    }

    char horizon_id[CBM_HORIZON_ID_MAX] = {0};
    char claim_id[CBM_CLAIM_ID_MAX] = {0};
    char dest_str[64] = {0};
    strncpy(horizon_id, yyjson_get_str(v_hid), sizeof(horizon_id) - 1);
    strncpy(claim_id, yyjson_get_str(v_cid), sizeof(claim_id) - 1);
    strncpy(dest_str, yyjson_get_str(v_dest), sizeof(dest_str) - 1);

    CbmSessionRegistry *sessions = get_sessions(srv);
    const CbmSessionHorizon *sh = cbm_session_get(sessions, horizon_id);
    if (!sh) {
        yyjson_doc_free(doc);
        cbm_refusal_emit(CBM_REFUSAL_CONTRACT_UNKNOWN, "union_claim_resolve",
                         "claims cannot be resolved outside an active open session");
        return json_error_result("SESSION_NOT_FOUND",
                                 "No active open session found for horizon_id; claims must be bound to a session");
    }

    CbmSessionSweepContext *sweep_ctx = cbm_mcp_get_session_sweep_context(horizon_id);
    if (!sweep_ctx) {
        yyjson_doc_free(doc);
        return json_error_result("SESSION_NOT_FOUND", "no sweep context found for active session");
    }

    CbmSweepDestination dest = CBM_SWEEP_DEST_UNRESOLVED;
    if (strcmp(dest_str, "PROMOTED") == 0) {
        dest = CBM_SWEEP_DEST_PROMOTED;
    } else if (strcmp(dest_str, "CONVERTED_OPEN_QUESTION") == 0) {
        dest = CBM_SWEEP_DEST_CONVERTED_OPEN_QUESTION;
    } else if (strcmp(dest_str, "DISCARDED") == 0) {
        dest = CBM_SWEEP_DEST_DISCARDED;
    } else {
        yyjson_doc_free(doc);
        return json_error_result("INVALID_PARAMS", "invalid destination enum");
    }

    yyjson_val *v_val = yyjson_obj_get(root, "validator_identity");
    if (!v_val) v_val = yyjson_obj_get(root, "decided_by");
    if (!v_val) v_val = yyjson_obj_get(root, "operator_identity");

    yyjson_val *v_tok = yyjson_obj_get(root, "operator_token");
    if (!v_tok) v_tok = yyjson_obj_get(root, "auth_token");
    if (!v_tok) v_tok = yyjson_obj_get(root, "token");

    yyjson_val *v_auto = yyjson_obj_get(root, "is_agent_autonomous");
    bool autonomous = (v_auto && yyjson_is_bool(v_auto)) ? yyjson_get_bool(v_auto) : false;

    if (v_val || v_tok) {
        const char *val_id = (v_val && yyjson_is_str(v_val)) ? yyjson_get_str(v_val) : NULL;
        const char *auth_tok = (v_tok && yyjson_is_str(v_tok)) ? yyjson_get_str(v_tok) : NULL;

        if (autonomous || !cbm_mcp_server_verify_operator_authority(srv, val_id, auth_tok)) {
            yyjson_doc_free(doc);
            cbm_refusal_emit(CBM_REFUSAL_AUTO_FOUNDING_FORBIDDEN, "union_claim_resolve",
                             "intent validation requires proven host operator authority; client self-declaration or autonomous adjudication is forbidden");
            return json_error_result("AUTO_VALIDATION_FORBIDDEN",
                                     "Intent validation requires proven host operator authority; client self-declaration or autonomous adjudication is forbidden");
        }

        const char *proven_op = cbm_mcp_server_get_operator_identity(srv);
        const char *effective_validator = (proven_op && proven_op[0]) ? proven_op : (val_id && val_id[0] ? val_id : "operator");

        char val_reason[256] = {0};
        CbmRefusalCode vrc = cbm_sweep_validate_intent(sweep_ctx, claim_id, effective_validator, val_reason, sizeof(val_reason));
        if (vrc != CBM_REFUSAL_OK) {
            yyjson_doc_free(doc);
            return json_error_result(cbm_refusal_code_string(vrc), val_reason[0] ? val_reason : "intent validation failed");
        }
    }

    char owner_or_reason_buf[128] = {0};
    const char *owner_or_reason = NULL;
    yyjson_val *v_or = yyjson_obj_get(root, "owner_or_reason");
    if (v_or && yyjson_is_str(v_or)) {
        strncpy(owner_or_reason_buf, yyjson_get_str(v_or), sizeof(owner_or_reason_buf) - 1);
        owner_or_reason = owner_or_reason_buf;
    }

    char reason[256] = {0};
    CbmRefusalCode rc = cbm_sweep_assign_destination(sweep_ctx, claim_id, dest, owner_or_reason, reason, sizeof(reason));
    yyjson_doc_free(doc);

    if (rc != CBM_REFUSAL_OK) {
        return json_error_result(cbm_refusal_code_string(rc), reason[0] ? reason : "failed to assign destination");
    }

    yyjson_mut_doc *out_doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *out_root = yyjson_mut_obj(out_doc);
    yyjson_mut_doc_set_root(out_doc, out_root);
    yyjson_mut_obj_add_bool(out_doc, out_root, "resolved", true);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "horizon_id", horizon_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "claim_id", claim_id);
    yyjson_mut_obj_add_strcpy(out_doc, out_root, "destination", dest_str);
    return result_from_mut_doc(out_doc, false);
}
