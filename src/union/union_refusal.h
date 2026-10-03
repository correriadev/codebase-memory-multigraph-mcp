/*
 * union_refusal.h — Typed Refusal Taxonomy (Scope A04).
 *
 * Refusals are the curriculum of the union: a typed refusal carries a
 * machine-readable code, a client obligation (the mandated behavior for the
 * submitting skill), and a log record. A refusal rendered as silence or as
 * empty success is the gravest lie available to the union.
 *
 * The taxonomy is closed and versioned: codes supersede, they never edit.
 * See docs/specs (harness-kit) SCOPE-A04 and ADR_V1 §3.4.
 */
#ifndef CBM_UNION_REFUSAL_H
#define CBM_UNION_REFUSAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CBM_REFUSAL_OK = 0,
    CBM_REFUSAL_CONTRACT_INVALID = 1,
    CBM_REFUSAL_CONTRACT_UNKNOWN = 2,
    CBM_REFUSAL_TOOL_UNCLASSIFIED = 3,
    CBM_REFUSAL_ANCHOR_NOT_FOUND = 4,
    CBM_REFUSAL_STALE_BASE = 5,
    CBM_REFUSAL_HORIZON_SKIP = 6,
    CBM_REFUSAL_ASSUMPTION_DROPPED = 7,
    CBM_REFUSAL_EVIDENCE_REQUIRED = 8,
    CBM_REFUSAL_BUDGET_EXHAUSTED = 9,
    CBM_REFUSAL_EXCLUSION_UNDECLARED = 10,
    CBM_REFUSAL_PROVENANCE_MISSING = 11,
    CBM_REFUSAL_CONTEST_UNPROVEN = 12,
    CBM_REFUSAL_SCOPE_EXCEEDED = 13,
    CBM_REFUSAL_RETRY_IDENTICAL = 14,
    CBM_REFUSAL_CLAIM_INVALID = 15,
    CBM_REFUSAL_PREDICATE_NOT_SELF_CONTAINED = 16,
    CBM_REFUSAL_SWEEP_INCOMPLETE = 17,
    CBM_REFUSAL_CODE_DOC_ASYMMETRY = 18,
    CBM_REFUSAL_LOG_REF_IMMUTABLE = 19,
    CBM_REFUSAL_PROVENANCE_UNDECLARED = 20,
    CBM_REFUSAL_BINDING_SELF_VALIDATED = 21,
    CBM_REFUSAL_TERRITORY_WRITE_FORBIDDEN = 22,
    CBM_REFUSAL_AUTO_FOUNDING_FORBIDDEN = 23,
    CBM_REFUSAL_THEME_SCHEMA_INVALID = 24,
    CBM_REFUSAL_THEME_UNKNOWN = 25,
    CBM_REFUSAL_THEME_PERSISTENCE_FAILED = 26,
    CBM_REFUSAL_THEME_VERSION_IMMUTABLE = 27,
    CBM_REFUSAL_CODE_COUNT = 28
} CbmRefusalCode;

/* Canonical machine-readable code string ("ANCHOR_NOT_FOUND", ...).
 * Never NULL for a valid code; "UNKNOWN" otherwise. */
const char *cbm_refusal_code_string(CbmRefusalCode code);

/* Inverse lookup. CBM_REFUSAL_OK when the string is not in the taxonomy —
 * unknown free-text refusals are a conformance failure, not a code. */
CbmRefusalCode cbm_refusal_code_from_string(const char *name);

/* The mandated client behavior for the code (non-NULL for valid codes). */
const char *cbm_refusal_client_obligation(CbmRefusalCode code);

/* True when the code is within the closed taxonomy. */
bool cbm_refusal_is_valid(CbmRefusalCode code);

/* ── Refusal emission (host log, never narrator) ──────────────────── */

/* Emit a structured refusal event to the host log:
 *   level=warn msg=union.refusal code=<CODE> obligation=<...> \
 *   horizon_id=<...> reason=<...>
 * Every refusal is logged as a refusal; there is no success-shaped variant
 * of this record. Reason may be NULL. */
void cbm_refusal_emit(CbmRefusalCode code, const char *horizon_id, const char *reason);

/* ── Retry-identical detection ─────────────────────────────────────── */

#define CBM_REFUSAL_LEDGER_CAP 256

typedef struct {
    uint64_t fingerprint; /* 0 = empty slot */
    CbmRefusalCode code;
    uint64_t recorded_at;
} CbmRefusalLedgerEntry;

typedef struct {
    CbmRefusalLedgerEntry entries[CBM_REFUSAL_LEDGER_CAP];
    size_t count;      /* slots ever used (bounded by cap) */
    size_t next_slot;  /* ring index: oldest entry is evicted when full */
} CbmRefusalLedger;

void cbm_refusal_ledger_init(CbmRefusalLedger *ledger);

/* FNV-1a 64 over horizon_id + code + payload. Content-based, cheap, and
 * deliberately NOT structural: structural detection is deferred until traces
 * show it paying (SCOPE-A04 open question 1). */
uint64_t cbm_refusal_fingerprint(const char *horizon_id, CbmRefusalCode code, const char *payload);

/* Record a refused submission. Returns true and sets *out_effective to
 * CBM_REFUSAL_RETRY_IDENTICAL when the identical submission (same horizon,
 * same code, same payload) was already refused — the re-refusal references
 * the original by fingerprint and is itself logged. A different payload, a
 * different horizon, or a different code records normally and returns false.
 * out_effective may be NULL. */
bool cbm_refusal_ledger_check_and_record(CbmRefusalLedger *ledger,
                                         const char *horizon_id,
                                         CbmRefusalCode code,
                                         const char *payload,
                                         CbmRefusalCode *out_effective);

#endif /* CBM_UNION_REFUSAL_H */
