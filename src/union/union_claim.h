/*
 * union_claim.h — Claim Node Anatomy & Validation (Scope C01).
 *
 * The documentary plane's atomic unit is the claim — a proposition,
 * not an AST symbol. A claim carries a single self-contained sentence,
 * strict provenance, typed anchor, consequence, and epistemic status.
 *
 * See docs/PRD/novos-paradgimas/specs/doc-plane/SCOPE-C01-claim-node-anatomy.md
 */
#ifndef CBM_UNION_CLAIM_H
#define CBM_UNION_CLAIM_H

#include "union_refusal.h"
#include "../core/symbolic_node.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_CLAIM_ID_MAX 64
#define CBM_CLAIM_PREDICATE_MAX 400
#define CBM_CLAIM_CONSEQUENCE_MAX 256
#define CBM_CLAIM_SESSION_MAX 64
#define CBM_CLAIM_HORIZON_MAX 64
#define CBM_CLAIM_PRINCIPAL_MAX 64
#define CBM_CLAIM_SEQ_MAX 64
#define CBM_CLAIM_REFS_CAP 8
#define CBM_CLAIM_REF_MAX 128
#define CBM_CLAIM_STORE_CAP 64

typedef enum {
    CBM_CLAIM_DECISION = 0,
    CBM_CLAIM_OPEN_QUESTION = 1,
    CBM_CLAIM_CONSTRAINT = 2,
    CBM_CLAIM_FACT = 3,
    CBM_CLAIM_VERDICT = 4,
    CBM_CLAIM_TYPE_COUNT = 5
} CbmClaimType;

const char *cbm_claim_type_string(CbmClaimType type);
CbmClaimType cbm_claim_type_from_string(const char *str);

typedef enum {
    CBM_ANCHOR_NONE = 0,
    CBM_ANCHOR_FILE_BYTES = 1,
    CBM_ANCHOR_LOG_REF = 2,
    CBM_ANCHOR_DERIVATION_REF = 3
} CbmAnchorKind;

const char *cbm_anchor_kind_string(CbmAnchorKind kind);

typedef struct {
    CbmAnchorKind kind;
    union {
        struct {
            char file_path[256];
            size_t byte_start;
            size_t byte_len;
            char expected_text[512];
        } file_bytes;
        struct {
            char session_id[CBM_CLAIM_SESSION_MAX];
            char horizon_id[CBM_CLAIM_HORIZON_MAX];
            uint64_t event_seq;
            uint64_t content_hash;
        } log_ref;
        struct {
            char generator_query_ref[256];
            uint64_t output_hash;
        } derivation_ref;
    };
} CbmAnchor;

typedef struct {
    char origin_session[CBM_CLAIM_SESSION_MAX];
    char origin_horizon[CBM_CLAIM_HORIZON_MAX];
    char proposed_by[CBM_CLAIM_PRINCIPAL_MAX];
    char validated_by[CBM_CLAIM_PRINCIPAL_MAX]; /* ∅ until operator validates */
} CbmClaimProvenance;

typedef struct {
    char id[CBM_CLAIM_ID_MAX];
    char predicate[CBM_CLAIM_PREDICATE_MAX];
    CbmClaimType type;
    EpistemicStatus status;
    CbmAnchor anchor;
    CbmClaimProvenance provenance;
    char consequence[CBM_CLAIM_CONSEQUENCE_MAX];
    char based_on_seq[CBM_CLAIM_SEQ_MAX];

    /* Evidence references (mandatory for VERDICT) */
    char evidence_refs[CBM_CLAIM_REFS_CAP][CBM_CLAIM_REF_MAX];
    size_t evidence_count;

    /* Realization slot (Scope C05) */
    bool realized_by_is_empty; /* true = pending slot ∅ */
    char realized_by_refs[CBM_CLAIM_REFS_CAP][CBM_CLAIM_REF_MAX];
    size_t realized_by_count;

    /* Flagged unresolved references (Scope C05) */
    bool has_unresolved_references;
    char unresolved_refs[CBM_CLAIM_REFS_CAP][CBM_CLAIM_REF_MAX];
    size_t unresolved_refs_count;

    /* Drift ladder statuses (Scope C06) */
    bool is_suspended;
    bool is_ungrounded;
} CbmClaim;

/* Deictic blacklist management */
void cbm_claim_blacklist_reset(void);
bool cbm_claim_blacklist_add(const char *phrase);
bool cbm_claim_is_deictic(const char *predicate, const char **out_matched_phrase);

/* Claim validation according to SCOPE-C01 rules:
 * - One sentence rule (<= 400 chars, no newline, single terminator)
 * - Deictic blacklist check (refusal PREDICATE_NOT_SELF_CONTAINED)
 * - Mandatory fields per claim type (refusal CLAIM_INVALID)
 * Returns CBM_REFUSAL_OK if valid, or the specific refusal code.
 * out_reason carries details (the invalid field name or the matched deictic phrase).
 */
CbmRefusalCode cbm_claim_validate(const CbmClaim *claim, char *out_reason, size_t reason_sz);

/* An in-memory horizon storage for claim nodes */
typedef struct {
    char horizon_id[CBM_CLAIM_HORIZON_MAX];
    CbmClaim claims[CBM_CLAIM_STORE_CAP];
    size_t count;
} CbmClaimStore;

void cbm_claim_store_init(CbmClaimStore *store, const char *horizon_id);

/* Submit claim to horizon store: validates claim first.
 * Upon success, inserts with status = EPISTEMIC_PROPOSED and returns CBM_REFUSAL_OK.
 * If invalid, emits refusal and returns refusal code.
 */
CbmRefusalCode cbm_claim_submit_to_horizon(CbmClaimStore *store,
                                          const CbmClaim *claim,
                                          char *out_reason,
                                          size_t reason_sz);

/* Retrieve claim by ID from store */
const CbmClaim *cbm_claim_store_get(const CbmClaimStore *store, const char *claim_id);

#endif /* CBM_UNION_CLAIM_H */
