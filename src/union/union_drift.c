/*
 * union_drift.c — Prose Drift Ladder (Scope C06).
 */
#include "union_drift.h"
#include "../foundation/log.h"

#include <stdio.h>
#include <string.h>

static const char *k_drift_kind_names[] = {
    "NONE",
    "LEXICAL",
    "STRUCTURAL",
    "GONE"
};

const char *cbm_drift_kind_string(CbmDriftKind kind) {
    if (kind >= 0 && kind <= CBM_DRIFT_GONE) {
        return k_drift_kind_names[kind];
    }
    return "UNKNOWN";
}

void cbm_drift_chain_init(CbmDriftScarChain *chain) {
    if (!chain) return;
    memset(chain, 0, sizeof(*chain));
}

void cbm_drift_chain_record(CbmDriftScarChain *chain,
                            const char *before_uri,
                            const char *after_uri,
                            CbmDriftKind kind,
                            uint64_t seq,
                            const char *note) {
    if (!chain || chain->count >= CBM_DRIFT_SCAR_CAP) return;

    CbmDriftScar *scar = &chain->scars[chain->count++];
    if (before_uri) {
        strncpy(scar->before_uri, before_uri, sizeof(scar->before_uri) - 1);
    }
    if (after_uri) {
        strncpy(scar->after_uri, after_uri, sizeof(scar->after_uri) - 1);
    }
    scar->kind = kind;
    scar->seq = seq;
    if (note) {
        strncpy(scar->note, note, sizeof(scar->note) - 1);
    }

    cbm_log_info("union.drift.scar",
                 "before", scar->before_uri,
                 "after", scar->after_uri,
                 "kind", cbm_drift_kind_string(kind),
                 NULL);
}

CbmRefusalCode cbm_drift_process_claim(CbmClaim *claim,
                                      CbmDriftKind kind,
                                      const char *new_uri_or_ref,
                                      uint64_t seq,
                                      const char *note,
                                      CbmDriftScarChain *chain,
                                      char *out_reason,
                                      size_t reason_sz) {
    if (!claim) return CBM_REFUSAL_CLAIM_INVALID;

    /* AC4: LOG_REF anchors are immutable; drift ladder refuses to process them */
    if (claim->anchor.kind == CBM_ANCHOR_LOG_REF) {
        if (out_reason && reason_sz > 0) {
            snprintf(out_reason, reason_sz,
                     "log-anchored claims are immutable and cannot drift; use epistemic supersession or recall");
        }
        cbm_refusal_emit(CBM_REFUSAL_LOG_REF_IMMUTABLE, claim->id,
                         out_reason ? out_reason : "log-anchored immutable ground");
        return CBM_REFUSAL_LOG_REF_IMMUTABLE;
    }

    if (kind == CBM_DRIFT_LEXICAL) {
        char old_uri[256];
        strncpy(old_uri, claim->id, sizeof(old_uri) - 1);
        old_uri[sizeof(old_uri) - 1] = '\0';

        if (new_uri_or_ref && new_uri_or_ref[0]) {
            strncpy(claim->id, new_uri_or_ref, sizeof(claim->id) - 1);
        }

        if (chain) {
            cbm_drift_chain_record(chain, old_uri, claim->id, CBM_DRIFT_LEXICAL, seq, note);
        }
        return CBM_REFUSAL_OK;
    }

    if (kind == CBM_DRIFT_STRUCTURAL) {
        claim->is_suspended = true;
        if (chain) {
            cbm_drift_chain_record(chain, claim->id, claim->id, CBM_DRIFT_STRUCTURAL, seq, note);
        }
        return CBM_REFUSAL_OK;
    }

    if (kind == CBM_DRIFT_GONE) {
        claim->is_ungrounded = true;
        if (chain) {
            cbm_drift_chain_record(chain, claim->id, "", CBM_DRIFT_GONE, seq, note);
        }
        return CBM_REFUSAL_OK;
    }

    return CBM_REFUSAL_OK;
}

const char *cbm_drift_resolve_uri(const CbmDriftScarChain *chain, const char *initial_uri) {
    if (!chain || !initial_uri) return initial_uri;

    const char *current = initial_uri;
    bool advanced = true;

    /* Chase chain without infinite loops */
    size_t iterations = 0;
    while (advanced && iterations < chain->count) {
        advanced = false;
        iterations++;
        for (size_t i = 0; i < chain->count; i++) {
            if (chain->scars[i].kind == CBM_DRIFT_LEXICAL &&
                strcmp(chain->scars[i].before_uri, current) == 0 &&
                chain->scars[i].after_uri[0] != '\0') {
                current = chain->scars[i].after_uri;
                advanced = true;
                break;
            }
        }
    }

    return current;
}

size_t cbm_drift_query_history(const CbmDriftScarChain *chain,
                               const char *uri,
                               CbmDriftScar *out_scars,
                               size_t max_scars) {
    if (!chain || !uri || !out_scars || max_scars == 0) return 0;

    size_t count = 0;
    const char *current = uri;

    for (size_t i = 0; i < chain->count && count < max_scars; i++) {
        if (strcmp(chain->scars[i].before_uri, current) == 0) {
            out_scars[count++] = chain->scars[i];
            if (chain->scars[i].after_uri[0] != '\0') {
                current = chain->scars[i].after_uri;
            }
        }
    }

    return count;
}
