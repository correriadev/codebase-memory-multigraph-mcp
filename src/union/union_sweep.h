/*
 * union_sweep.h — Conversational Birth, Intent Validation & Closure Sweep (Scope C03).
 *
 * Captures conversational claims into the session horizon, enforces operator-only
 * intent validation for DECISION / OPEN_QUESTION, and guarantees every PROPOSED claim
 * reaches a mandatory destination at closure or emits SWEEP_INCOMPLETE.
 *
 * See docs/PRD/novos-paradgimas/specs/doc-plane/SCOPE-C03-conversational-birth-sweep.md
 */
#ifndef CBM_UNION_SWEEP_H
#define CBM_UNION_SWEEP_H

#include "union_claim.h"
#include "union_anchor.h"
#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CBM_SWEEP_DEST_UNRESOLVED = 0,
    CBM_SWEEP_DEST_PROMOTED = 1,
    CBM_SWEEP_DEST_CONVERTED_OPEN_QUESTION = 2,
    CBM_SWEEP_DEST_DISCARDED = 3
} CbmSweepDestination;

typedef struct {
    size_t promoted_count;
    size_t converted_count;
    size_t discarded_exploration_count;
    size_t discarded_dead_end_count;
    size_t discarded_total_count;
} CbmSweepReport;

typedef struct {
    char session_id[CBM_CLAIM_SESSION_MAX];
    char horizon_id[CBM_CLAIM_HORIZON_MAX];
    CbmClaim claims[CBM_CLAIM_STORE_CAP];
    CbmSweepDestination destinations[CBM_CLAIM_STORE_CAP];
    char destination_notes[CBM_CLAIM_STORE_CAP][64];
    size_t claim_count;
    bool is_closed;
    CbmSweepReport report;
} CbmSessionSweepContext;

void cbm_sweep_init(CbmSessionSweepContext *ctx,
                    const char *session_id,
                    const char *horizon_id);

/* Capture conversational claim:
 * - proposed_by = "agent"
 * - validated_by = ∅
 * - status = EPISTEMIC_PROPOSED
 * - anchor = LOG_REF to exchange_seq
 * - announces transparency event (logged)
 */
CbmRefusalCode cbm_sweep_capture_claim(CbmSessionSweepContext *ctx,
                                      CbmClaim *claim,
                                      uint64_t exchange_seq,
                                      char *out_reason,
                                      size_t reason_sz);

/* Operator validates intent of a captured claim.
 * Gated: only validator_identity == "operator" (or "operator_*" prefixed) is accepted.
 * If agent attempts self-validation -> refusal CLAIM_INVALID naming validated_by.
 */
CbmRefusalCode cbm_sweep_validate_intent(CbmSessionSweepContext *ctx,
                                        const char *claim_id,
                                        const char *validator_identity,
                                        char *out_reason,
                                        size_t reason_sz);

/* Assign mandatory destination before session closure:
 * - PROMOTED (promotes through gate; if DECISION/OPEN_QUESTION, validated_by must be set)
 * - CONVERTED_OPEN_QUESTION (requires owner)
 * - DISCARDED (requires typed reason, e.g. "exploration" or "dead_end")
 */
CbmRefusalCode cbm_sweep_assign_destination(CbmSessionSweepContext *ctx,
                                           const char *claim_id,
                                           CbmSweepDestination dest,
                                           const char *owner_or_reason,
                                           char *out_reason,
                                           size_t reason_sz);

/* Attempt session closure:
 * Checks that all claims have a destination assigned.
 * If any PROPOSED claim remains UNRESOLVED:
 * - Emits SWEEP_INCOMPLETE
 * - Fills out_unresolved_list with claim IDs
 * - Blocks closure (returns CBM_REFUSAL_SWEEP_INCOMPLETE)
 * When all claims resolved:
 * - Computes typed exclusion counts (discarded claims are never stored as content)
 * - Marks is_closed = true
 * - Returns CBM_REFUSAL_OK
 */
CbmRefusalCode cbm_sweep_close_session(CbmSessionSweepContext *ctx,
                                       const CbmAnchorEnv *anchor_env,
                                       char *out_unresolved_list,
                                       size_t list_sz,
                                       char *out_reason,
                                       size_t reason_sz);

/* Find claim in sweep context */
CbmClaim *cbm_sweep_find_claim(CbmSessionSweepContext *ctx, const char *claim_id);

#endif /* CBM_UNION_SWEEP_H */
