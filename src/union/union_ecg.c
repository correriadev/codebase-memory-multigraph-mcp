/*
 * union_ecg.c — Aging & Orphan Queries (The ECG) (Scope C07).
 */
#include "union_ecg.h"
#include "../foundation/log.h"

#include <stdio.h>
#include <string.h>

size_t cbm_ecg_query_aging_intentions(const CbmClaim *claims,
                                      size_t claim_count,
                                      const uint64_t *claim_ages_days,
                                      uint64_t min_age_days,
                                      const char *owner_filter,
                                      CbmAgingIntentionResult *out_results,
                                      size_t max_results) {
    if (!claims || !out_results || max_results == 0) return 0;

    size_t count = 0;
    for (size_t i = 0; i < claim_count && count < max_results; i++) {
        const CbmClaim *c = &claims[i];

        /* Only unrealized claims (REALIZED_BY = ∅) constitute intention debt */
        if (!c->realized_by_is_empty) {
            continue;
        }

        uint64_t age = claim_ages_days ? claim_ages_days[i] : 0;
        if (age < min_age_days) {
            continue;
        }

        if (owner_filter && owner_filter[0]) {
            bool matches_validator = (strcmp(c->provenance.validated_by, owner_filter) == 0);
            bool matches_proposer = (strcmp(c->provenance.proposed_by, owner_filter) == 0);
            if (!matches_validator && !matches_proposer) {
                continue;
            }
        }

        CbmAgingIntentionResult *res = &out_results[count++];
        strncpy(res->claim_id, c->id, sizeof(res->claim_id) - 1);
        strncpy(res->predicate, c->predicate, sizeof(res->predicate) - 1);
        res->type = c->type;
        res->status = c->status;
        strncpy(res->consequence, c->consequence, sizeof(res->consequence) - 1);
        strncpy(res->validated_by, c->provenance.validated_by, sizeof(res->validated_by) - 1);
        strncpy(res->proposed_by, c->provenance.proposed_by, sizeof(res->proposed_by) - 1);
        res->age_days = age;
        res->seq_distance = 0;
    }

    return count;
}

size_t cbm_ecg_query_orphan_realizations(const CbmCodeSymbolRegistry *code_reg,
                                         const CbmDocEdgeStore *edge_store,
                                         const char *territory_filter,
                                         CbmOrphanRealizationResult *out_results,
                                         size_t max_results) {
    if (!code_reg || !out_results || max_results == 0) return 0;

    size_t count = 0;
    for (size_t i = 0; i < code_reg->count && count < max_results; i++) {
        const char *symbol = code_reg->symbols[i];

        if (territory_filter && territory_filter[0]) {
            if (!strstr(symbol, territory_filter)) {
                continue;
            }
        }

        /* Check if any doc claim realized into this code symbol */
        bool is_realized = false;
        if (edge_store) {
            for (size_t e = 0; e < edge_store->count; e++) {
                if (edge_store->edges[e].type == CBM_DOC_EDGE_REALIZED_BY &&
                    strcmp(edge_store->edges[e].target, symbol) == 0) {
                    is_realized = true;
                    break;
                }
            }
        }

        if (!is_realized) {
            CbmOrphanRealizationResult *res = &out_results[count++];
            strncpy(res->code_symbol, symbol, sizeof(res->code_symbol) - 1);
            /* territory inferred from file path prefix */
            const char *slash = strrchr(symbol, '/');
            if (slash) {
                size_t terr_len = (size_t)(slash - symbol);
                if (terr_len >= sizeof(res->territory)) terr_len = sizeof(res->territory) - 1;
                memcpy(res->territory, symbol, terr_len);
                res->territory[terr_len] = '\0';
            } else {
                strncpy(res->territory, "root", sizeof(res->territory) - 1);
            }
            res->created_at_seq = 0;
        }
    }

    return count;
}

void cbm_ecg_compute_metrics(const CbmClaim *claims,
                             size_t claim_count,
                             const uint64_t *claim_ages_days,
                             const CbmCodeSymbolRegistry *code_reg,
                             const CbmDocEdgeStore *edge_store,
                             CbmEcgMetrics *out_metrics) {
    if (!out_metrics) return;
    memset(out_metrics, 0, sizeof(*out_metrics));

    if (claims) {
        for (size_t i = 0; i < claim_count; i++) {
            if (!claims[i].realized_by_is_empty) {
                continue;
            }

            uint64_t age = claim_ages_days ? claim_ages_days[i] : 0;
            if (age < 30) {
                out_metrics->intention_debt_fresh++;
            } else if (age <= 90) {
                out_metrics->intention_debt_aging++;
            } else {
                out_metrics->intention_debt_critical++;
            }

            if (claims[i].provenance.validated_by[0] != '\0') {
                out_metrics->intention_debt_operator_validated++;
            } else {
                out_metrics->intention_debt_model_assumed++;
            }
        }
    }

    if (code_reg) {
        CbmOrphanRealizationResult tmp[CBM_ECG_RESULTS_CAP];
        out_metrics->orphan_realizations_count =
            cbm_ecg_query_orphan_realizations(code_reg, edge_store, NULL, tmp, CBM_ECG_RESULTS_CAP);
    }

    cbm_log_info("union.ecg.metrics",
                 "intention_fresh", out_metrics->intention_debt_fresh ? "count" : "0",
                 "intention_aging", out_metrics->intention_debt_aging ? "count" : "0",
                 "intention_critical", out_metrics->intention_debt_critical ? "count" : "0",
                 NULL);
}
