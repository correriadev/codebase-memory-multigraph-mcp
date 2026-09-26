/*
 * union_doc_edges.h — Referential Resolution L1 & Edge Families (Scope C05).
 *
 * Edge families:
 * - Birth: DERIVES_FROM (doc -> log/trace)
 * - Rhetorical: SUPPORTS, CONTRADICTS, REFINES, SUPERSEDES (doc -> doc)
 * - Realization: REFERENCES, REALIZED_BY (doc -> code ONLY)
 *
 * Realization pending slot: REALIZED_BY = ∅
 * Invariant: Never code -> doc edges (Asymmetry).
 * Reverse index: code symbol -> doc claims.
 *
 * See docs/PRD/novos-paradgimas/specs/doc-plane/SCOPE-C05-referential-resolution-edges.md
 */
#ifndef CBM_UNION_DOC_EDGES_H
#define CBM_UNION_DOC_EDGES_H

#include "union_claim.h"
#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CBM_DOC_EDGE_DERIVES_FROM = 0,
    CBM_DOC_EDGE_SUPPORTS = 1,
    CBM_DOC_EDGE_CONTRADICTS = 2,
    CBM_DOC_EDGE_REFINES = 3,
    CBM_DOC_EDGE_SUPERSEDES = 4,
    CBM_DOC_EDGE_REFERENCES = 5,
    CBM_DOC_EDGE_REALIZED_BY = 6,
    CBM_DOC_EDGE_TYPE_COUNT = 7
} CbmDocEdgeType;

const char *cbm_doc_edge_type_string(CbmDocEdgeType type);

#define CBM_DOC_EDGES_CAP 256
#define CBM_CODE_SYMBOLS_CAP 128

typedef struct {
    char source[256];
    char target[256];
    CbmDocEdgeType type;
    char scar[128];
    uint64_t created_at_seq;
} CbmDocEdge;

typedef struct {
    char symbols[CBM_CODE_SYMBOLS_CAP][128];
    size_t count;
} CbmCodeSymbolRegistry;

void cbm_code_registry_init(CbmCodeSymbolRegistry *reg);
bool cbm_code_registry_add(CbmCodeSymbolRegistry *reg, const char *symbol);
bool cbm_code_registry_contains(const CbmCodeSymbolRegistry *reg, const char *symbol);

typedef struct {
    CbmDocEdge edges[CBM_DOC_EDGES_CAP];
    size_t count;
} CbmDocEdgeStore;

void cbm_doc_edge_store_init(CbmDocEdgeStore *store);

/* Add doc edge with asymmetry invariant check:
 * Code -> doc edge returns CBM_REFUSAL_CODE_DOC_ASYMMETRY.
 */
CbmRefusalCode cbm_doc_edge_add(CbmDocEdgeStore *store,
                                const CbmCodeSymbolRegistry *code_reg,
                                const CbmDocEdge *edge,
                                char *out_reason,
                                size_t reason_sz);

/* L1 Referential resolution:
 * Resolves backtick-quoted symbols or cbm:// symbols in predicate against code_reg.
 * Resolvable -> REFERENCES edge in edge_store.
 * Unresolved -> flags UNRESOLVED_REFERENCE on claim.
 */
CbmRefusalCode cbm_doc_resolve_l1_references(CbmClaim *claim,
                                            const CbmCodeSymbolRegistry *code_reg,
                                            CbmDocEdgeStore *edge_store);

/* Realization event:
 * Fills REALIZED_BY slot with code_claim_ref.
 * Claim status remains unchanged.
 */
CbmRefusalCode cbm_doc_apply_realization(CbmClaim *claim,
                                         const char *code_claim_ref,
                                         CbmDocEdgeStore *edge_store,
                                         uint64_t seq);

/* Supersession event:
 * Claim A supersedes Claim B with scar. B status transitions to EPISTEMIC_SHADOWED.
 */
CbmRefusalCode cbm_doc_apply_supersession(CbmClaim *claim_a,
                                          CbmClaim *claim_b,
                                          const char *scar,
                                          CbmDocEdgeStore *edge_store,
                                          uint64_t seq);

/* Reverse Index Query:
 * Finds all doc edges referencing code_symbol.
 */
size_t cbm_doc_reverse_query_symbol(const CbmDocEdgeStore *edge_store,
                                    const char *code_symbol,
                                    CbmDocEdge *out_edges,
                                    size_t max_edges);

#endif /* CBM_UNION_DOC_EDGES_H */
