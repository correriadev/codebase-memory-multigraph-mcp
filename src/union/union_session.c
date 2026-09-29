/*
 * union_session.c — Session Horizon Protocol (Scope A01).
 */
#include "union_session.h"
#include "mutation_journal.h"

#include "../foundation/log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

const char *cbm_session_close_reason_string(CbmSessionCloseReason reason) {
    switch (reason) {
    case CBM_SESSION_CLOSE_NORMAL: return "NORMAL";
    case CBM_SESSION_CLOSE_ABNORMAL: return "ABNORMAL";
    case CBM_SESSION_CLOSE_EMPTY: return "EMPTY";
    }
    return "UNKNOWN";
}

void cbm_session_registry_init(CbmSessionRegistry *registry) {
    if (!registry) return;
    memset(registry, 0, sizeof(*registry));
}

static CbmSessionHorizon *find_session(CbmSessionRegistry *sessions, const char *horizon_id) {
    if (!sessions || !horizon_id) return NULL;
    for (size_t i = 0; i < sessions->count; i++) {
        if (strcmp(sessions->sessions[i].horizon_id, horizon_id) == 0) {
            return &sessions->sessions[i];
        }
    }
    return NULL;
}

const CbmSessionHorizon *cbm_session_get(const CbmSessionRegistry *sessions,
                                        const char *horizon_id) {
    return find_session((CbmSessionRegistry *)sessions, horizon_id);
}

static bool session_context_matches(const CbmHostWorkContext *left,
                                    const CbmHostWorkContext *right) {
    return left && right && left->host == right->host &&
           strcmp(left->context_id, right->context_id) == 0;
}

static bool session_grounding_matches(const CbmChangeGrounding *left,
                                      const CbmChangeGrounding *right) {
    return left && right && left->kind == right->kind &&
           strcmp(left->intent_key, right->intent_key) == 0 &&
           strcmp(left->reference, right->reference) == 0 &&
           strcmp(left->rationale, right->rationale) == 0;
}

CbmSessionResult cbm_session_bind_mutation_context(CbmSessionRegistry *sessions,
                                                   const char *horizon_id,
                                                   const CbmHostWorkContext *host_context,
                                                   const CbmChangeGrounding *grounding,
                                                   char *out_error, size_t err_sz) {
    CbmSessionHorizon *session = find_session(sessions, horizon_id);
    if (!session) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "SESSION_NOT_FOUND");
        return CBM_SESSION_ERR_NOT_FOUND;
    }
    if (!cbm_host_work_context_is_valid(host_context) ||
        !cbm_change_grounding_is_valid(grounding)) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "INVALID_MUTATION_CONTEXT");
        return CBM_SESSION_ERR_INVALID_CONTEXT;
    }
    if (session->has_bound_context) {
        if (session_context_matches(&session->host_context, host_context) &&
            session->has_change_grounding &&
            session_grounding_matches(&session->change_grounding, grounding)) {
            return CBM_SESSION_OK;
        }
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "SESSION_CONTEXT_ALREADY_BOUND");
        return CBM_SESSION_ERR_CONTEXT_ALREADY_BOUND;
    }

    session->host_context = *host_context;
    session->change_grounding = *grounding;
    session->has_bound_context = true;
    session->has_change_grounding = true;
    cbm_log_info("union.session.context_bound",
                 "horizon_id", session->horizon_id,
                 "host", session->host_context.host == CBM_MUTATION_HOST_CODEX
                             ? "codex" : "antigravity",
                 "context_id", session->host_context.context_id,
                 "intent_key", session->change_grounding.intent_key,
                 NULL);
    if (out_error && err_sz > 0) out_error[0] = '\0';
    return CBM_SESSION_OK;
}

const CbmSessionHorizon *cbm_session_find_bound_context(
    const CbmSessionRegistry *sessions, const CbmHostWorkContext *host_context) {
    if (!sessions || !cbm_host_work_context_is_valid(host_context)) return NULL;
    for (size_t i = 0; i < sessions->count; i++) {
        const CbmSessionHorizon *session = &sessions->sessions[i];
        if (session->has_bound_context && session->has_change_grounding &&
            session_context_matches(&session->host_context, host_context)) {
            return session;
        }
    }
    return NULL;
}

CbmSessionResult cbm_session_open(CbmSessionRegistry *sessions,
                                 CbmContractRegistry *contracts,
                                 HorizonConnectionPool *pool,
                                 uint32_t client_pid,
                                 const char *skill_identity,
                                 const char *contract_id,
                                 const char *based_on_seq,
                                 CbmSessionHorizon *out,
                                 char *out_error, size_t err_sz) {
    if (!sessions || !pool || !skill_identity || !out) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "NULL_ARGUMENT");
        return CBM_SESSION_ERR_NULL;
    }
    if (sessions->count >= CBM_SESSION_REGISTRY_CAP) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "SESSION_REGISTRY_FULL");
        return CBM_SESSION_ERR_FULL;
    }

    memset(out, 0, sizeof(*out));

    /* Contract lookup: unknown identity is restricted mode, recorded — never
     * refused outright (SCOPE-A01 AC2). */
    const CbmSkillContract *contract = NULL;
    if (contract_id && contract_id[0]) {
        contract = cbm_contract_registry_get(contracts, contract_id);
    }
    out->restricted = (contract == NULL);
    out->open_refusal = out->restricted ? CBM_REFUSAL_CONTRACT_UNKNOWN : CBM_REFUSAL_OK;

    /* Horizon creation via the existing pool (isolated DB, DDL, metadata). */
    char horizon_id[CBM_HORIZON_ID_MAX];
    if (cbm_create_horizon(pool, client_pid, NULL, based_on_seq ? based_on_seq : "0",
                           horizon_id, sizeof(horizon_id)) != 0) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "HORIZON_CREATE_FAILED");
        return CBM_SESSION_ERR_POOL;
    }

    snprintf(out->horizon_id, sizeof(out->horizon_id), "%s", horizon_id);
    snprintf(out->identity, sizeof(out->identity), "%s", skill_identity);
    if (contract_id) snprintf(out->contract_id, sizeof(out->contract_id), "%s", contract_id);
    snprintf(out->based_on_seq, sizeof(out->based_on_seq), "%s",
             based_on_seq ? based_on_seq : "0");
    out->opened_at_unix = (uint64_t)time(NULL);
    out->actions = 0;
    out->refusals = 0;

    sessions->sessions[sessions->count++] = *out;

    /* The open event is the host-log record: identity, horizon, mode,
     * belief-sequence. Restricted mode is visible here, by construction. */
    cbm_log_info("union.session.open",
                 "horizon_id", out->horizon_id,
                 "identity", out->identity,
                 "contract_id", out->contract_id,
                 "restricted", out->restricted ? "true" : "false",
                 "open_refusal", cbm_refusal_code_string(out->open_refusal),
                 "based_on_seq", out->based_on_seq,
                 NULL);
    if (out->restricted) {
        cbm_refusal_emit(CBM_REFUSAL_CONTRACT_UNKNOWN, out->horizon_id,
                         "no registered contract; session restricted to irreversible-class gating");
    }

    return CBM_SESSION_OK;
}

CbmSessionResult cbm_session_record_action(CbmSessionRegistry *sessions,
                                           const char *horizon_id) {
    CbmSessionHorizon *s = find_session(sessions, horizon_id);
    if (!s) return CBM_SESSION_ERR_NOT_FOUND;
    s->actions++;
    return CBM_SESSION_OK;
}

CbmSessionResult cbm_session_record_refusal(CbmSessionRegistry *sessions,
                                            const char *horizon_id) {
    CbmSessionHorizon *s = find_session(sessions, horizon_id);
    if (!s) return CBM_SESSION_ERR_NOT_FOUND;
    s->refusals++;
    return CBM_SESSION_OK;
}

CbmSessionResult cbm_session_close(CbmSessionRegistry *sessions,
                                   HorizonConnectionPool *pool,
                                   const char *horizon_id,
                                   CbmSessionCloseReason reason,
                                   CbmSessionClosure *out_closure,
                                   char *out_error, size_t err_sz) {
    CbmSessionHorizon *s = find_session(sessions, horizon_id);
    if (!s) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "SESSION_NOT_FOUND");
        return CBM_SESSION_ERR_NOT_FOUND;
    }
    if (!pool) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "NULL_ARGUMENT");
        return CBM_SESSION_ERR_NULL;
    }
    if (s->has_bound_context &&
        cbm_mutation_journal_mark_default_session_unknown(s->horizon_id) !=
            CBM_MUTATION_JOURNAL_OK) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "MUTATION_JOURNAL_UNAVAILABLE");
        return CBM_SESSION_ERR_JOURNAL;
    }

    uint64_t now = (uint64_t)time(NULL);
    uint64_t duration = now >= s->opened_at_unix ? now - s->opened_at_unix : 0;

    /* Closure event FIRST: the event survives; the content does not. */
    char actions_buf[16], refusals_buf[16], duration_buf[16];
    snprintf(actions_buf, sizeof(actions_buf), "%u", s->actions);
    snprintf(refusals_buf, sizeof(refusals_buf), "%u", s->refusals);
    snprintf(duration_buf, sizeof(duration_buf), "%llu", (unsigned long long)duration);
    cbm_log_info("union.session.close",
                 "horizon_id", s->horizon_id,
                 "identity", s->identity,
                 "reason", cbm_session_close_reason_string(reason),
                 "actions", actions_buf,
                 "refusals", refusals_buf,
                 "duration_sec", duration_buf,
                 NULL);

    if (out_closure) {
        memset(out_closure, 0, sizeof(*out_closure));
        snprintf(out_closure->horizon_id, sizeof(out_closure->horizon_id), "%s", s->horizon_id);
        snprintf(out_closure->identity, sizeof(out_closure->identity), "%s", s->identity);
        snprintf(out_closure->reason, sizeof(out_closure->reason), "%s",
                 cbm_session_close_reason_string(reason));
        out_closure->duration_sec = duration;
        out_closure->actions = s->actions;
        out_closure->refusals = s->refusals;
    }

    /* Destroy the horizon content (events survive in the log). */
    cbm_discard_horizon(pool, s->horizon_id);

    /* Remove the session from the registry (swap-compact). */
    size_t idx = (size_t)(s - sessions->sessions);
    sessions->sessions[idx] = sessions->sessions[sessions->count - 1];
    sessions->count--;

    return CBM_SESSION_OK;
}
