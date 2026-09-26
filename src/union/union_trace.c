/*
 * union_trace.c — Factual Session Traces (Scope A09).
 */
#include "union_trace.h"

#include <stdio.h>
#include <string.h>

void cbm_trace_init(CbmSessionTrace *trace, const char *horizon_id, const char *identity, const char *contract_id) {
    if (!trace) return;
    memset(trace, 0, sizeof(*trace));
    if (horizon_id) snprintf(trace->horizon_id, sizeof(trace->horizon_id), "%s", horizon_id);
    if (identity) snprintf(trace->identity, sizeof(trace->identity), "%s", identity);
    if (contract_id) snprintf(trace->contract_id, sizeof(trace->contract_id), "%s", contract_id);
    snprintf(trace->outcome, sizeof(trace->outcome), "PENDING");
}

void cbm_trace_record_refusal(CbmSessionTrace *trace, CbmRefusalCode code) {
    if (!trace) return;
    if (trace->refusal_count < CBM_TRACE_REFUSALS_CAP) {
        trace->refusal_codes[trace->refusal_count++] = code;
    }
}

void cbm_trace_reconcile_ledger(CbmSessionTrace *trace, const CbmHorizonLedger *ledger) {
    if (!trace || !ledger) return;
    trace->actions_idempotent = ledger->actions_idempotent;
    trace->actions_compensable = ledger->actions_compensable;
    trace->actions_irreversible = ledger->actions_irreversible;
    trace->actions_total = trace->actions_idempotent + trace->actions_compensable + trace->actions_irreversible;
}

void cbm_trace_reconcile_promotion(CbmSessionTrace *trace, const CbmPromotionProposal *proposal, const char *outcome) {
    if (!trace) return;
    if (proposal) {
        trace->exclusions = proposal->exclusions;
    }
    if (outcome) {
        snprintf(trace->outcome, sizeof(trace->outcome), "%s", outcome);
    }
}

int cbm_trace_format_evaluator_json(const CbmSessionTrace *trace, char *out_buf, size_t buf_sz) {
    if (!trace || !out_buf || buf_sz == 0) return -1;

    /* Narrative is excluded by construction per A09 AC3 */
    int written = snprintf(out_buf, buf_sz,
        "{\"horizon_id\":\"%s\",\"identity\":\"%s\",\"contract_id\":\"%s\","
        "\"based_on_seq\":\"%s\",\"outcome\":\"%s\",\"actions\":{\"total\":%u,"
        "\"idempotent\":%u,\"compensable\":%u,\"irreversible\":%u},"
        "\"refusals_count\":%zu,\"exclusions\":{\"declared\":%s,\"anchor_failed\":%u,"
        "\"coverage_open\":%u,\"contested\":%u,\"out_of_scope\":%u,\"budget_truncated\":%u}}",
        trace->horizon_id, trace->identity, trace->contract_id,
        trace->based_on_seq, trace->outcome, trace->actions_total,
        trace->actions_idempotent, trace->actions_compensable, trace->actions_irreversible,
        trace->refusal_count, trace->exclusions.declared ? "true" : "false",
        trace->exclusions.counts[CBM_EXCLUSION_ANCHOR_FAILED],
        trace->exclusions.counts[CBM_EXCLUSION_COVERAGE_OPEN],
        trace->exclusions.counts[CBM_EXCLUSION_CONTESTED],
        trace->exclusions.counts[CBM_EXCLUSION_OUT_OF_SCOPE],
        trace->exclusions.counts[CBM_EXCLUSION_BUDGET_TRUNCATED]
    );

    return (written > 0 && (size_t)written < buf_sz) ? 0 : -1;
}
