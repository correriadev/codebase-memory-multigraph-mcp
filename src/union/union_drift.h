/*
 * union_drift.h — Prose Drift Ladder (Scope C06).
 *
 * Mutation semantics for documentary anchors:
 * - lexical: heading renamed / moved, text preserved -> scar recorded, no demotion, URI alias resolved
 * - structural: bytes changed -> node suspended (quarantined) until re-grounded
 * - gone: section/file deleted -> node demotes to ungrounded / source
 * - LOG_REF immutability: log-anchored claims CANNOT drift (LOG_REF_IMMUTABLE refusal)
 *
 * See docs/PRD/novos-paradgimas/specs/doc-plane/SCOPE-C06-prose-drift-ladder.md
 */
#ifndef CBM_UNION_DRIFT_H
#define CBM_UNION_DRIFT_H

#include "union_claim.h"
#include "union_anchor.h"
#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CBM_DRIFT_NONE = 0,
    CBM_DRIFT_LEXICAL = 1,
    CBM_DRIFT_STRUCTURAL = 2,
    CBM_DRIFT_GONE = 3
} CbmDriftKind;

const char *cbm_drift_kind_string(CbmDriftKind kind);

#define CBM_DRIFT_SCAR_CAP 64

typedef struct {
    char before_uri[256];
    char after_uri[256];
    CbmDriftKind kind;
    uint64_t seq;
    char note[128];
} CbmDriftScar;

typedef struct {
    CbmDriftScar scars[CBM_DRIFT_SCAR_CAP];
    size_t count;
} CbmDriftScarChain;

void cbm_drift_chain_init(CbmDriftScarChain *chain);

void cbm_drift_chain_record(CbmDriftScarChain *chain,
                            const char *before_uri,
                            const char *after_uri,
                            CbmDriftKind kind,
                            uint64_t seq,
                            const char *note);

/* Process drift on a claim.
 * Gated: LOG_REF anchors cannot drift -> returns CBM_REFUSAL_LOG_REF_IMMUTABLE.
 * FILE_BYTES anchors classify into lexical / structural / gone.
 */
CbmRefusalCode cbm_drift_process_claim(CbmClaim *claim,
                                      CbmDriftKind kind,
                                      const char *new_uri_or_ref,
                                      uint64_t seq,
                                      const char *note,
                                      CbmDriftScarChain *chain,
                                      char *out_reason,
                                      size_t reason_sz);

/* Chase lexical renames across the scar chain */
const char *cbm_drift_resolve_uri(const CbmDriftScarChain *chain, const char *initial_uri);

/* Query full scar history for a given node / URI */
size_t cbm_drift_query_history(const CbmDriftScarChain *chain,
                               const char *uri,
                               CbmDriftScar *out_scars,
                               size_t max_scars);

#endif /* CBM_UNION_DRIFT_H */
