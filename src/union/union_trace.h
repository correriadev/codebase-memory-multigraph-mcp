/*
 * union_trace.h — Factual Session Traces (Scope A09).
 *
 * Emitted at horizon closure. Factual, append-only, and separated from graph.
 * Quarantines narrative text so evaluators consume only facts and host-log verified metrics.
 */
#ifndef CBM_UNION_TRACE_H
#define CBM_UNION_TRACE_H

#include "union_ledger.h"
#include "union_promotion.h"
#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_TRACE_ID_MAX 64
#define CBM_TRACE_SUMMARY_MAX 128
#define CBM_TRACE_NARRATIVE_MAX 256
#define CBM_TRACE_REFUSALS_CAP 16

typedef struct {
    char horizon_id[CBM_TRACE_ID_MAX];
    char identity[CBM_TRACE_ID_MAX];
    char contract_id[CBM_TRACE_ID_MAX];
    char based_on_seq[CBM_TRACE_ID_MAX];
    char task_summary[CBM_TRACE_SUMMARY_MAX];
    char outcome[32];

    uint64_t opened_at_unix;
    uint64_t closed_at_unix;

    /* Metrics verified against ledger & gateway */
    uint32_t actions_total;
    uint32_t actions_idempotent;
    uint32_t actions_compensable;
    uint32_t actions_irreversible;

    /* Refusals received during session */
    CbmRefusalCode refusal_codes[CBM_TRACE_REFUSALS_CAP];
    size_t refusal_count;

    /* Exclusions declared during promotion */
    CbmExclusionSummary exclusions;

    /* Quarantined narrative — never exposed to automated evaluators */
    char quarantined_narrative[CBM_TRACE_NARRATIVE_MAX];
} CbmSessionTrace;

void cbm_trace_init(CbmSessionTrace *trace, const char *horizon_id, const char *identity, const char *contract_id);

void cbm_trace_record_refusal(CbmSessionTrace *trace, CbmRefusalCode code);

void cbm_trace_reconcile_ledger(CbmSessionTrace *trace, const CbmHorizonLedger *ledger);

void cbm_trace_reconcile_promotion(CbmSessionTrace *trace, const CbmPromotionProposal *proposal, const char *outcome);

/* Formats the evaluator-safe JSON trace (omits quarantined_narrative by construction). */
int cbm_trace_format_evaluator_json(const CbmSessionTrace *trace, char *out_buf, size_t buf_sz);

#endif /* CBM_UNION_TRACE_H */
