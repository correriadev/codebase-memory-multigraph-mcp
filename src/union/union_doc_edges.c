/*
 * union_doc_edges.c — Referential Resolution L1 & Edge Families (Scope C05).
 */
#include "union_doc_edges.h"
#include "../foundation/log.h"

#include <stdio.h>
#include <string.h>

static const char *k_doc_edge_type_names[] = {
    "DERIVES_FROM",
    "SUPPORTS",
    "CONTRADICTS",
    "REFINES",
    "SUPERSEDES",
    "REFERENCES",
    "REALIZED_BY"
};

const char *cbm_doc_edge_type_string(CbmDocEdgeType type) {
    if (type >= 0 && type < CBM_DOC_EDGE_TYPE_COUNT) {
        return k_doc_edge_type_names[type];
    }
    return "UNKNOWN";
}

void cbm_code_registry_init(CbmCodeSymbolRegistry *reg) {
    if (!reg) return;
    memset(reg, 0, sizeof(*reg));
}

bool cbm_code_registry_add(CbmCodeSymbolRegistry *reg, const char *symbol) {
    if (!reg || !symbol || !symbol[0]) return false;
    if (reg->count >= CBM_CODE_SYMBOLS_CAP) return false;
    strncpy(reg->symbols[reg->count], symbol, sizeof(reg->symbols[0]) - 1);
    reg->symbols[reg->count][sizeof(reg->symbols[0]) - 1] = '\0';
    reg->count++;
    return true;
}

bool cbm_code_registry_contains(const CbmCodeSymbolRegistry *reg, const char *symbol) {
    if (!reg || !symbol) return false;
    for (size_t i = 0; i < reg->count; i++) {
        if (strcmp(reg->symbols[i], symbol) == 0) {
            return true;
        }
    }
    return false;
}

void cbm_doc_edge_store_init(CbmDocEdgeStore *store) {
    if (!store) return;
    memset(store, 0, sizeof(*store));
}

CbmRefusalCode cbm_doc_edge_add(CbmDocEdgeStore *store,
                                const CbmCodeSymbolRegistry *code_reg,
                                const CbmDocEdge *edge,
                                char *out_reason,
                                size_t reason_sz) {
    if (!store || !edge) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "null store or edge");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    /* Asymmetry check: code nodes may NEVER point to doc nodes */
    bool source_is_code = (strncmp(edge->source, "code://", 7) == 0) ||
                          (code_reg && cbm_code_registry_contains(code_reg, edge->source));

    bool target_is_doc = (strncmp(edge->target, "claim_", 6) == 0) ||
                         (strncmp(edge->target, "cbm://docs/", 11) == 0);

    if (source_is_code && target_is_doc) {
        if (out_reason && reason_sz > 0) {
            snprintf(out_reason, reason_sz, "asymmetry invariant violated: code-to-doc edge prohibited (%s -> %s)",
                     edge->source, edge->target);
        }
        cbm_refusal_emit(CBM_REFUSAL_CODE_DOC_ASYMMETRY, "doc_edge_store",
                         out_reason ? out_reason : "asymmetry violation");
        return CBM_REFUSAL_CODE_DOC_ASYMMETRY;
    }

    if (store->count >= CBM_DOC_EDGES_CAP) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "doc edge store full");
        return CBM_REFUSAL_BUDGET_EXHAUSTED;
    }

    store->edges[store->count++] = *edge;
    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_doc_resolve_l1_references(CbmClaim *claim,
                                            const CbmCodeSymbolRegistry *code_reg,
                                            CbmDocEdgeStore *edge_store) {
    if (!claim) return CBM_REFUSAL_CLAIM_INVALID;

    const char *p = claim->predicate;
    while (*p) {
        const char *open_tick = strchr(p, '`');
        if (!open_tick) break;
        const char *close_tick = strchr(open_tick + 1, '`');
        if (!close_tick) break;

        size_t len = (size_t)(close_tick - open_tick - 1);
        if (len > 0 && len < 128) {
            char symbol[128];
            memcpy(symbol, open_tick + 1, len);
            symbol[len] = '\0';

            if (code_reg && cbm_code_registry_contains(code_reg, symbol)) {
                if (edge_store) {
                    CbmDocEdge edge = {0};
                    strncpy(edge.source, claim->id, sizeof(edge.source) - 1);
                    strncpy(edge.target, symbol, sizeof(edge.target) - 1);
                    edge.type = CBM_DOC_EDGE_REFERENCES;
                    cbm_doc_edge_add(edge_store, code_reg, &edge, NULL, 0);
                }
            } else {
                /* Nonexistent symbol cited: flag UNRESOLVED_REFERENCE */
                claim->has_unresolved_references = true;
                if (claim->unresolved_refs_count < CBM_CLAIM_REFS_CAP) {
                    strncpy(claim->unresolved_refs[claim->unresolved_refs_count], symbol,
                            sizeof(claim->unresolved_refs[0]) - 1);
                    claim->unresolved_refs_count++;
                }
            }
        }
        p = close_tick + 1;
    }

    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_doc_apply_realization(CbmClaim *claim,
                                         const char *code_claim_ref,
                                         CbmDocEdgeStore *edge_store,
                                         uint64_t seq) {
    if (!claim || !code_claim_ref) return CBM_REFUSAL_CLAIM_INVALID;

    claim->realized_by_is_empty = false;
    if (claim->realized_by_count < CBM_CLAIM_REFS_CAP) {
        strncpy(claim->realized_by_refs[claim->realized_by_count], code_claim_ref,
                sizeof(claim->realized_by_refs[0]) - 1);
        claim->realized_by_count++;
    }

    /* Edge created */
    if (edge_store) {
        CbmDocEdge edge = {0};
        strncpy(edge.source, claim->id, sizeof(edge.source) - 1);
        strncpy(edge.target, code_claim_ref, sizeof(edge.target) - 1);
        edge.type = CBM_DOC_EDGE_REALIZED_BY;
        edge.created_at_seq = seq;
        cbm_doc_edge_add(edge_store, NULL, &edge, NULL, 0);
    }

    cbm_log_info("union.claim.realized",
                 "claim_id", claim->id,
                 "code_ref", code_claim_ref,
                 "seq", seq ? "seq" : "",
                 NULL);

    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_doc_apply_supersession(CbmClaim *claim_a,
                                          CbmClaim *claim_b,
                                          const char *scar,
                                          CbmDocEdgeStore *edge_store,
                                          uint64_t seq) {
    if (!claim_a || !claim_b) return CBM_REFUSAL_CLAIM_INVALID;

    claim_b->status = EPISTEMIC_SHADOWED;

    if (edge_store) {
        CbmDocEdge edge = {0};
        strncpy(edge.source, claim_a->id, sizeof(edge.source) - 1);
        strncpy(edge.target, claim_b->id, sizeof(edge.target) - 1);
        edge.type = CBM_DOC_EDGE_SUPERSEDES;
        if (scar) {
            strncpy(edge.scar, scar, sizeof(edge.scar) - 1);
        }
        edge.created_at_seq = seq;
        cbm_doc_edge_add(edge_store, NULL, &edge, NULL, 0);
    }

    cbm_log_info("union.claim.superseded",
                 "successor", claim_a->id,
                 "superseded", claim_b->id,
                 "scar", scar ? scar : "",
                 NULL);

    return CBM_REFUSAL_OK;
}

size_t cbm_doc_reverse_query_symbol(const CbmDocEdgeStore *edge_store,
                                    const char *code_symbol,
                                    CbmDocEdge *out_edges,
                                    size_t max_edges) {
    if (!edge_store || !code_symbol || !out_edges || max_edges == 0) return 0;

    size_t count = 0;
    for (size_t i = 0; i < edge_store->count && count < max_edges; i++) {
        if (strcmp(edge_store->edges[i].target, code_symbol) == 0) {
            out_edges[count++] = edge_store->edges[i];
        }
    }
    return count;
}
