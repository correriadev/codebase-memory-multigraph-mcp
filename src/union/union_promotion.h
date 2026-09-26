/*
 * union_promotion.h — Promotion with Named Exclusion & Blind Gate (Scope A07 / A08).
 *
 * Enforces:
 * - Named exclusion: rejected claims/paths must be typed and counted; silence is refused (EXCLUSION_UNDECLARED).
 * - Empty promotion: valid, first-class outcome (PROMOTION_EMPTY) when all candidates are excluded.
 * - Single-step horizon crossing: target must match the direct DAG parent; skipping is refused (HORIZON_SKIP).
 * - Assumption conservation: inherited assumptions must be carried or explicitly resolved (ASSUMPTION_DROPPED).
 * - Caller-blindness: submitter identity is metadata; verdict depends strictly on content evidence.
 */
#ifndef CBM_UNION_PROMOTION_H
#define CBM_UNION_PROMOTION_H

#include "union_ledger.h"
#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_EXCLUSION_TYPES_COUNT 5
#define CBM_PROMOTION_ASSUMPTIONS_CAP 16
#define CBM_PROMOTION_ASSUMPTION_LEN 64
#define CBM_PROMOTION_ID_MAX 64

typedef enum {
    CBM_EXCLUSION_ANCHOR_FAILED = 0,
    CBM_EXCLUSION_COVERAGE_OPEN = 1,
    CBM_EXCLUSION_CONTESTED = 2,
    CBM_EXCLUSION_OUT_OF_SCOPE = 3,
    CBM_EXCLUSION_BUDGET_TRUNCATED = 4
} CbmExclusionType;

const char *cbm_exclusion_type_string(CbmExclusionType type);

typedef struct {
    bool declared;
    uint32_t counts[CBM_EXCLUSION_TYPES_COUNT];
    char exhaustion_ref[CBM_PROMOTION_ID_MAX]; /* required when budget_truncated > 0 */
} CbmExclusionSummary;

typedef struct {
    char submitter_identity[CBM_PROMOTION_ID_MAX];
    char source_horizon_id[CBM_PROMOTION_ID_MAX];
    char target_horizon_id[CBM_PROMOTION_ID_MAX];
    char expected_dag_parent[CBM_PROMOTION_ID_MAX];

    size_t candidate_count;
    size_t promoted_count;

    CbmExclusionSummary exclusions;

    char recorded_assumptions[CBM_PROMOTION_ASSUMPTIONS_CAP][CBM_PROMOTION_ASSUMPTION_LEN];
    size_t recorded_assumptions_count;

    char resolved_assumptions[CBM_PROMOTION_ASSUMPTIONS_CAP][CBM_PROMOTION_ASSUMPTION_LEN];
    size_t resolved_assumptions_count;

    bool has_forged_authority;
} CbmPromotionProposal;

typedef enum {
    CBM_PROMOTION_ADMITTED = 0,
    CBM_PROMOTION_EMPTY = 1,
    CBM_PROMOTION_REFUSED = 2
} CbmPromotionOutcome;

CbmPromotionOutcome cbm_promotion_evaluate(const CbmPromotionProposal *proposal,
                                          const CbmHorizonLedger *ledger,
                                          CbmRefusalCode *out_refusal,
                                          char *out_reason, size_t reason_sz);

#endif /* CBM_UNION_PROMOTION_H */
