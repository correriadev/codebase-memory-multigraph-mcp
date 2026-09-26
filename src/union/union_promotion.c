/*
 * union_promotion.c — Promotion with Named Exclusion & Blind Gate (Scope A07 / A08).
 */
#include "union_promotion.h"

#include <stdio.h>
#include <string.h>

const char *cbm_exclusion_type_string(CbmExclusionType type) {
    switch (type) {
        case CBM_EXCLUSION_ANCHOR_FAILED: return "anchor_failed";
        case CBM_EXCLUSION_COVERAGE_OPEN: return "coverage_open";
        case CBM_EXCLUSION_CONTESTED: return "contested";
        case CBM_EXCLUSION_OUT_OF_SCOPE: return "out_of_scope";
        case CBM_EXCLUSION_BUDGET_TRUNCATED: return "budget_truncated";
        default: return "unknown";
    }
}

CbmPromotionOutcome cbm_promotion_evaluate(const CbmPromotionProposal *proposal,
                                          const CbmHorizonLedger *ledger,
                                          CbmRefusalCode *out_refusal,
                                          char *out_reason, size_t reason_sz) {
    if (out_refusal) *out_refusal = CBM_REFUSAL_OK;
    if (out_reason && reason_sz > 0) out_reason[0] = '\0';

    if (!proposal) {
        if (out_refusal) *out_refusal = CBM_REFUSAL_CONTRACT_INVALID;
        return CBM_PROMOTION_REFUSED;
    }

    /* 1. Ledger exhaustion invariant (FR-14): exhaustion never promotes */
    if (ledger && !cbm_ledger_can_promote(ledger)) {
        if (out_refusal) *out_refusal = CBM_REFUSAL_BUDGET_EXHAUSTED;
        if (out_reason && reason_sz > 0) {
            snprintf(out_reason, reason_sz, "ledger_exhausted; promotion_blocked");
        }
        cbm_refusal_emit(CBM_REFUSAL_BUDGET_EXHAUSTED, proposal->source_horizon_id, "ledger_exhausted");
        return CBM_PROMOTION_REFUSED;
    }

    /* 2. Forged authority check (A08 AC3) */
    if (proposal->has_forged_authority) {
        if (out_refusal) *out_refusal = CBM_REFUSAL_PROVENANCE_MISSING;
        if (out_reason && reason_sz > 0) {
            snprintf(out_reason, reason_sz, "forged_authority_ref");
        }
        cbm_refusal_emit(CBM_REFUSAL_PROVENANCE_MISSING, proposal->source_horizon_id, "forged_authority_ref");
        return CBM_PROMOTION_REFUSED;
    }

    /* 3. Horizon topology check: promotion crosses exactly one boundary (A07 AC4) */
    if (proposal->expected_dag_parent[0] &&
        strcmp(proposal->target_horizon_id, proposal->expected_dag_parent) != 0) {
        if (out_refusal) *out_refusal = CBM_REFUSAL_HORIZON_SKIP;
        if (out_reason && reason_sz > 0) {
            snprintf(out_reason, reason_sz, "horizon_skip:target=%s!=expected_parent=%s",
                     proposal->target_horizon_id, proposal->expected_dag_parent);
        }
        cbm_refusal_emit(CBM_REFUSAL_HORIZON_SKIP, proposal->source_horizon_id, "horizon_skip");
        return CBM_PROMOTION_REFUSED;
    }

    /* 4. Named exclusion declaration check (A07 AC1) */
    if (!proposal->exclusions.declared) {
        if (out_refusal) *out_refusal = CBM_REFUSAL_EXCLUSION_UNDECLARED;
        if (out_reason && reason_sz > 0) {
            snprintf(out_reason, reason_sz, "exclusion_summary_missing");
        }
        cbm_refusal_emit(CBM_REFUSAL_EXCLUSION_UNDECLARED, proposal->source_horizon_id, "exclusion_summary_missing");
        return CBM_PROMOTION_REFUSED;
    }

    /* 5. Assumption conservation check (A07 AC5) */
    for (size_t i = 0; i < proposal->recorded_assumptions_count; i++) {
        const char *rec = proposal->recorded_assumptions[i];
        bool accounted = false;
        for (size_t j = 0; j < proposal->resolved_assumptions_count; j++) {
            if (strcmp(rec, proposal->resolved_assumptions[j]) == 0) {
                accounted = true;
                break;
            }
        }
        if (!accounted) {
            if (out_refusal) *out_refusal = CBM_REFUSAL_ASSUMPTION_DROPPED;
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz, "assumption_dropped:%s", rec);
            }
            cbm_refusal_emit(CBM_REFUSAL_ASSUMPTION_DROPPED, proposal->source_horizon_id, rec);
            return CBM_PROMOTION_REFUSED;
        }
    }

    /* 6. Empty promotion check (A07 AC3) */
    if (proposal->promoted_count == 0) {
        fprintf(stderr, "level=info msg=union.promotion.empty horizon_id=%s\n",
                proposal->source_horizon_id);
        return CBM_PROMOTION_EMPTY;
    }

    /* 7. Promotion admitted */
    fprintf(stderr, "level=info msg=union.promotion.admitted horizon_id=%s target=%s promoted=%zu\n",
            proposal->source_horizon_id, proposal->target_horizon_id, proposal->promoted_count);
    return CBM_PROMOTION_ADMITTED;
}
