/*
 * union_sweep.c — Conversational Birth, Intent Validation & Closure Sweep (Scope C03).
 */
#include "union_sweep.h"
#include "../foundation/log.h"

#include <stdio.h>
#include <string.h>

void cbm_sweep_init(CbmSessionSweepContext *ctx,
                    const char *session_id,
                    const char *horizon_id) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    if (session_id) {
        strncpy(ctx->session_id, session_id, sizeof(ctx->session_id) - 1);
    }
    if (horizon_id) {
        strncpy(ctx->horizon_id, horizon_id, sizeof(ctx->horizon_id) - 1);
    }
}

CbmClaim *cbm_sweep_find_claim(CbmSessionSweepContext *ctx, const char *claim_id) {
    if (!ctx || !claim_id) return NULL;
    for (size_t i = 0; i < ctx->claim_count; i++) {
        if (strcmp(ctx->claims[i].id, claim_id) == 0) {
            return &ctx->claims[i];
        }
    }
    return NULL;
}

CbmRefusalCode cbm_sweep_capture_claim(CbmSessionSweepContext *ctx,
                                      CbmClaim *claim,
                                      uint64_t exchange_seq,
                                      char *out_reason,
                                      size_t reason_sz) {
    if (!ctx || !claim) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "null ctx or claim");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    /* Enforce conversational birth invariants */
    strncpy(claim->provenance.origin_session, ctx->session_id,
            sizeof(claim->provenance.origin_session) - 1);
    strncpy(claim->provenance.origin_horizon, ctx->horizon_id,
            sizeof(claim->provenance.origin_horizon) - 1);
    strncpy(claim->provenance.proposed_by, "agent",
            sizeof(claim->provenance.proposed_by) - 1);
    claim->provenance.validated_by[0] = '\0'; /* ∅ */
    claim->status = EPISTEMIC_PROPOSED;

    /* Anchor to host log exchange */
    claim->anchor.kind = CBM_ANCHOR_LOG_REF;
    strncpy(claim->anchor.log_ref.session_id, ctx->session_id,
            sizeof(claim->anchor.log_ref.session_id) - 1);
    strncpy(claim->anchor.log_ref.horizon_id, ctx->horizon_id,
            sizeof(claim->anchor.log_ref.horizon_id) - 1);
    claim->anchor.log_ref.event_seq = exchange_seq;
    claim->anchor.log_ref.content_hash = cbm_anchor_hash_text(claim->predicate);

    CbmRefusalCode val = cbm_claim_validate(claim, out_reason, reason_sz);
    if (val != CBM_REFUSAL_OK) {
        cbm_refusal_emit(val, ctx->horizon_id, out_reason ? out_reason : "");
        return val;
    }

    if (ctx->claim_count >= CBM_CLAIM_STORE_CAP) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "sweep store full");
        return CBM_REFUSAL_BUDGET_EXHAUSTED;
    }

    ctx->claims[ctx->claim_count] = *claim;
    ctx->destinations[ctx->claim_count] = CBM_SWEEP_DEST_UNRESOLVED;
    ctx->destination_notes[ctx->claim_count][0] = '\0';
    ctx->claim_count++;

    /* Transparency event: announced to operator */
    cbm_log_info("union.claim.captured",
                 "horizon_id", ctx->horizon_id,
                 "claim_id", claim->id,
                 "status", "PROPOSED",
                 "type", cbm_claim_type_string(claim->type),
                 NULL);

    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_sweep_validate_intent(CbmSessionSweepContext *ctx,
                                        const char *claim_id,
                                        const char *validator_identity,
                                        char *out_reason,
                                        size_t reason_sz) {
    if (!ctx || !claim_id) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "null ctx or claim_id");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    /* Self-validation prevention rule:
     * Only "operator" or an operator_* principal is valid.
     * Agent, assistant, bot, self, etc. are rejected. */
    if (!validator_identity ||
        strncmp(validator_identity, "operator", 8) != 0) {
        if (out_reason && reason_sz > 0) {
            snprintf(out_reason, reason_sz,
                     "intent validation requires operator (self-validation prohibited on validated_by)");
        }
        cbm_refusal_emit(CBM_REFUSAL_CLAIM_INVALID, ctx->horizon_id,
                         "self-validation attempted on validated_by");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    CbmClaim *claim = cbm_sweep_find_claim(ctx, claim_id);
    if (!claim) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "claim not found: %s", claim_id);
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    strncpy(claim->provenance.validated_by, validator_identity,
            sizeof(claim->provenance.validated_by) - 1);

    cbm_log_info("union.claim.validated",
                 "horizon_id", ctx->horizon_id,
                 "claim_id", claim_id,
                 "validated_by", validator_identity,
                 NULL);

    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_sweep_assign_destination(CbmSessionSweepContext *ctx,
                                           const char *claim_id,
                                           CbmSweepDestination dest,
                                           const char *owner_or_reason,
                                           char *out_reason,
                                           size_t reason_sz) {
    if (!ctx || !claim_id) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "null ctx or claim_id");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    size_t idx = (size_t)-1;
    for (size_t i = 0; i < ctx->claim_count; i++) {
        if (strcmp(ctx->claims[i].id, claim_id) == 0) {
            idx = i;
            break;
        }
    }
    if (idx == (size_t)-1) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "claim %s not found", claim_id);
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    CbmClaim *claim = &ctx->claims[idx];

    if (dest == CBM_SWEEP_DEST_PROMOTED) {
        /* DECISION and OPEN_QUESTION require validated_by before promotion */
        if (claim->type == CBM_CLAIM_DECISION || claim->type == CBM_CLAIM_OPEN_QUESTION) {
            if (!claim->provenance.validated_by[0]) {
                if (out_reason && reason_sz > 0) {
                    snprintf(out_reason, reason_sz,
                             "intent validation required before promotion: missing validated_by");
                }
                cbm_refusal_emit(CBM_REFUSAL_CLAIM_INVALID, ctx->horizon_id,
                                 "intent validation required for DECISION/OPEN_QUESTION");
                return CBM_REFUSAL_CLAIM_INVALID;
            }
        }
        claim->status = EPISTEMIC_ACCEPTED;
    } else if (dest == CBM_SWEEP_DEST_CONVERTED_OPEN_QUESTION) {
        claim->type = CBM_CLAIM_OPEN_QUESTION;
        if (owner_or_reason && owner_or_reason[0]) {
            strncpy(claim->provenance.proposed_by, owner_or_reason,
                    sizeof(claim->provenance.proposed_by) - 1);
        }
    }

    ctx->destinations[idx] = dest;
    if (owner_or_reason) {
        strncpy(ctx->destination_notes[idx], owner_or_reason,
                sizeof(ctx->destination_notes[0]) - 1);
    }

    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_sweep_close_session(CbmSessionSweepContext *ctx,
                                       const CbmAnchorEnv *anchor_env,
                                       char *out_unresolved_list,
                                       size_t list_sz,
                                       char *out_reason,
                                       size_t reason_sz) {
    if (!ctx) return CBM_REFUSAL_CLAIM_INVALID;

    size_t unresolved_count = 0;
    if (out_unresolved_list && list_sz > 0) out_unresolved_list[0] = '\0';

    for (size_t i = 0; i < ctx->claim_count; i++) {
        if (ctx->destinations[i] == CBM_SWEEP_DEST_UNRESOLVED) {
            unresolved_count++;
            if (out_unresolved_list && list_sz > 0) {
                if (out_unresolved_list[0] != '\0') {
                    strncat(out_unresolved_list, ", ", list_sz - strlen(out_unresolved_list) - 1);
                }
                strncat(out_unresolved_list, ctx->claims[i].id,
                        list_sz - strlen(out_unresolved_list) - 1);
            }
        }
    }

    if (unresolved_count > 0) {
        if (out_reason && reason_sz > 0) {
            snprintf(out_reason, reason_sz, "%zu proposed claims unresolved: %s",
                     unresolved_count, out_unresolved_list ? out_unresolved_list : "");
        }
        cbm_refusal_emit(CBM_REFUSAL_SWEEP_INCOMPLETE, ctx->horizon_id,
                         out_unresolved_list ? out_unresolved_list : "unresolved claims");
        return CBM_REFUSAL_SWEEP_INCOMPLETE;
    }

    /* Compute destination report */
    memset(&ctx->report, 0, sizeof(ctx->report));
    for (size_t i = 0; i < ctx->claim_count; i++) {
        switch (ctx->destinations[i]) {
            case CBM_SWEEP_DEST_PROMOTED:
                ctx->report.promoted_count++;
                break;
            case CBM_SWEEP_DEST_CONVERTED_OPEN_QUESTION:
                ctx->report.converted_count++;
                break;
            case CBM_SWEEP_DEST_DISCARDED:
                ctx->report.discarded_total_count++;
                if (strstr(ctx->destination_notes[i], "dead_end")) {
                    ctx->report.discarded_dead_end_count++;
                } else {
                    ctx->report.discarded_exploration_count++;
                }
                /* Discarded claims appear as typed exclusion counts — never content */
                ctx->claims[i].predicate[0] = '\0';
                ctx->claims[i].consequence[0] = '\0';
                break;
            default:
                break;
        }
    }

    ctx->is_closed = true;
    cbm_log_info("union.sweep.closed",
                 "session_id", ctx->session_id,
                 "horizon_id", ctx->horizon_id,
                 NULL);

    return CBM_REFUSAL_OK;
}
