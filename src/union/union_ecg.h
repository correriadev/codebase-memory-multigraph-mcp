/*
 * union_ecg.h — Aging & Orphan Queries (The ECG) (Scope C07).
 *
 * Query 1: aging intention (admitted claims with REALIZED_BY = ∅)
 * Query 2: orphan realization (code without realizing doc claim)
 * Metrics emission: intention-debt and orphan counts by age bucket and territory.
 *
 * See docs/PRD/novos-paradgimas/specs/doc-plane/SCOPE-C07-aging-orphan-queries.md
 */
#ifndef CBM_UNION_ECG_H
#define CBM_UNION_ECG_H

#include "union_claim.h"
#include "union_doc_edges.h"
#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_ECG_RESULTS_CAP 64

typedef struct {
    char claim_id[CBM_CLAIM_ID_MAX];
    char predicate[CBM_CLAIM_PREDICATE_MAX];
    CbmClaimType type;
    EpistemicStatus status;
    char consequence[CBM_CLAIM_CONSEQUENCE_MAX];
    char validated_by[CBM_CLAIM_PRINCIPAL_MAX];
    char proposed_by[CBM_CLAIM_PRINCIPAL_MAX];
    uint64_t age_days;
    uint64_t seq_distance;
} CbmAgingIntentionResult;

typedef struct {
    char code_symbol[128];
    char territory[128];
    uint64_t created_at_seq;
} CbmOrphanRealizationResult;

typedef struct {
    size_t intention_debt_fresh;     /* < 30 days */
    size_t intention_debt_aging;     /* 30..90 days */
    size_t intention_debt_critical;  /* > 90 days */
    size_t intention_debt_operator_validated;
    size_t intention_debt_model_assumed;
    size_t orphan_realizations_count;
} CbmEcgMetrics;

/* Query 1: Aging intention (admitted claims with REALIZED_BY = ∅)
 * Filterable by min_age_days and optional owner_filter.
 * Results carry consequence, status, validated_by, proposed_by.
 */
size_t cbm_ecg_query_aging_intentions(const CbmClaim *claims,
                                      size_t claim_count,
                                      const uint64_t *claim_ages_days,
                                      uint64_t min_age_days,
                                      const char *owner_filter,
                                      CbmAgingIntentionResult *out_results,
                                      size_t max_results);

/* Query 2: Orphan realization (code claims with no reverse REALIZED_BY)
 * Filterable by optional territory_filter (prefix match on code symbol or territory).
 */
size_t cbm_ecg_query_orphan_realizations(const CbmCodeSymbolRegistry *code_reg,
                                         const CbmDocEdgeStore *edge_store,
                                         const char *territory_filter,
                                         CbmOrphanRealizationResult *out_results,
                                         size_t max_results);

/* Compute aggregated ECG metrics from log-computable events */
void cbm_ecg_compute_metrics(const CbmClaim *claims,
                             size_t claim_count,
                             const uint64_t *claim_ages_days,
                             const CbmCodeSymbolRegistry *code_reg,
                             const CbmDocEdgeStore *edge_store,
                             CbmEcgMetrics *out_metrics);

#endif /* CBM_UNION_ECG_H */
