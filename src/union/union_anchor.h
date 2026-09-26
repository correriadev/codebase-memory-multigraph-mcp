/*
 * union_anchor.h — Anchor Kinds & Ground Verification (Scope C02).
 *
 * Three birth natures:
 * 1. FILE_BYTES: (file_path, byte_start, byte_len, expected_text)
 * 2. LOG_REF: (session_id, horizon_id, event_seq, content_hash) - immutable ground
 * 3. DERIVATION_REF: (generator_query_ref, output_hash)
 *
 * See docs/PRD/novos-paradgimas/specs/doc-plane/SCOPE-C02-anchor-kinds.md
 */
#ifndef CBM_UNION_ANCHOR_H
#define CBM_UNION_ANCHOR_H

#include "union_claim.h"
#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CBM_ANCHOR_VERIFY_OK = 0,
    CBM_ANCHOR_VERIFY_NOT_FOUND = 1,
    CBM_ANCHOR_VERIFY_INVALID_DERIVATION = 2,
    CBM_ANCHOR_VERIFY_MALFORMED = 3,
    CBM_ANCHOR_VERIFY_DRIFTED = 4
} CbmAnchorVerifyStatus;

/* In-memory mockable stores for deterministic testing and runtime resolution */

#define CBM_LOG_STORE_CAP 128
#define CBM_LOG_TEXT_MAX 512

typedef struct {
    char session_id[CBM_CLAIM_SESSION_MAX];
    char horizon_id[CBM_CLAIM_HORIZON_MAX];
    uint64_t event_seq;
    char text[CBM_LOG_TEXT_MAX];
    uint64_t content_hash;
} CbmLogEvent;

typedef struct {
    CbmLogEvent events[CBM_LOG_STORE_CAP];
    size_t count;
} CbmLogStore;

void cbm_log_store_init(CbmLogStore *store);
bool cbm_log_store_append(CbmLogStore *store,
                          const char *session_id,
                          const char *horizon_id,
                          uint64_t event_seq,
                          const char *text);
const CbmLogEvent *cbm_log_store_find(const CbmLogStore *store,
                                      const char *session_id,
                                      uint64_t event_seq);

#define CBM_DERIVATION_STORE_CAP 64

typedef struct {
    char query_ref[256];
    uint64_t current_hash;
} CbmDerivationEntry;

typedef struct {
    CbmDerivationEntry entries[CBM_DERIVATION_STORE_CAP];
    size_t count;
} CbmDerivationStore;

void cbm_derivation_store_init(CbmDerivationStore *store);
bool cbm_derivation_store_set(CbmDerivationStore *store,
                              const char *query_ref,
                              uint64_t output_hash);

/* Combined environment for anchor verification */
typedef struct {
    const CbmLogStore *log_store;
    const CbmDerivationStore *derivation_store;
    /* Optional file reader override (NULL uses stdio / filesystem) */
    bool (*file_reader)(const char *path, size_t start, size_t len,
                        char *out_buf, size_t buf_sz, void *ctx);
    void *file_reader_ctx;
} CbmAnchorEnv;

/* Validates anchor structure (is it well-formed and non-empty?)
 * Returns CBM_REFUSAL_OK if well-formed, or CBM_REFUSAL_CLAIM_INVALID naming "anchor".
 */
CbmRefusalCode cbm_anchor_validate_structure(const CbmAnchor *anchor,
                                            char *out_reason,
                                            size_t reason_sz);

/* Verifies an anchor against the real (log store, filesystem, or derivation).
 * Returns CbmAnchorVerifyStatus and sets out_refusal if failed:
 * - CBM_REFUSAL_OK on verify success
 * - CBM_REFUSAL_ANCHOR_NOT_FOUND if ground not found / drifted
 * - CBM_REFUSAL_CLAIM_INVALID if malformed
 */
CbmAnchorVerifyStatus cbm_anchor_verify(const CbmAnchor *anchor,
                                        const CbmAnchorEnv *env,
                                        CbmRefusalCode *out_refusal,
                                        char *out_reason,
                                        size_t reason_sz);

/* Hash helper for anchor content (FNV-1a 64) */
uint64_t cbm_anchor_hash_text(const char *text);

#endif /* CBM_UNION_ANCHOR_H */
