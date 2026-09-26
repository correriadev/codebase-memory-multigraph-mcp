/*
 * union_binding.c — Binding Claims (DEVE/PODE) & Deviation Ledger (Scope D02).
 */
#include "union_binding.h"
#include "../foundation/log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

void cbm_binding_ledger_init(CbmBindingLedger *ledger) {
    if (!ledger) return;
    memset(ledger, 0, sizeof(*ledger));
}

CbmRefusalCode cbm_binding_validate_and_admit(CbmBindingLedger *ledger,
                                              const CbmBindingClaim *binding,
                                              char *err_reason,
                                              size_t err_len) {
    if (!ledger || !binding) {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "null ledger or binding");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    if (binding->theme_id[0] == '\0') {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "missing mandatory field: theme_id");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    if (binding->pinned_version[0] == '\0') {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "missing mandatory field: pinned_version");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    /* Agent cannot self-validate a normative or consulted binding */
    if (binding->validated_by[0] != '\0') {
        if (strstr(binding->validated_by, "agent") != NULL) {
            if (err_reason && err_len > 0) {
                snprintf(err_reason, err_len, "theme bindings require operator validation; self-validation by agent is forbidden");
            }
            cbm_refusal_emit(CBM_REFUSAL_BINDING_SELF_VALIDATED, "binding_ledger", "agent attempted self-validation");
            return CBM_REFUSAL_BINDING_SELF_VALIDATED;
        }
    } else if (binding->mode == CBM_BINDING_NORMATIVE) {
        /* Normative (DEVE) binding mandates operator validation */
        if (err_reason && err_len > 0) {
            snprintf(err_reason, err_len, "normative binding mandates operator validation");
        }
        cbm_refusal_emit(CBM_REFUSAL_BINDING_SELF_VALIDATED, "binding_ledger", "normative binding missing operator validation");
        return CBM_REFUSAL_BINDING_SELF_VALIDATED;
    }

    /* Update if existing theme binding */
    for (size_t i = 0; i < ledger->binding_count; i++) {
        if (strcmp(ledger->bindings[i].theme_id, binding->theme_id) == 0) {
            ledger->bindings[i] = *binding;
            ledger->bindings[i].updated_at = (uint64_t)time(NULL);
            return CBM_REFUSAL_OK;
        }
    }

    if (ledger->binding_count >= CBM_BINDING_CAP) {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "binding ledger capacity exceeded");
        return CBM_REFUSAL_SCOPE_EXCEEDED;
    }

    ledger->bindings[ledger->binding_count] = *binding;
    ledger->bindings[ledger->binding_count].created_at = (uint64_t)time(NULL);
    ledger->bindings[ledger->binding_count].updated_at = ledger->bindings[ledger->binding_count].created_at;
    ledger->binding_count++;

    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_binding_lookup(const CbmBindingLedger *ledger,
                                 const char *theme_id,
                                 CbmBindingClaim *out_binding) {
    if (!ledger || !theme_id) return CBM_REFUSAL_THEME_UNKNOWN;

    for (size_t i = 0; i < ledger->binding_count; i++) {
        if (strcmp(ledger->bindings[i].theme_id, theme_id) == 0) {
            if (out_binding) *out_binding = ledger->bindings[i];
            return CBM_REFUSAL_OK;
        }
    }
    return CBM_REFUSAL_THEME_UNKNOWN;
}

CbmRefusalCode cbm_deviation_admit(CbmBindingLedger *ledger,
                                   const CbmDeviationClaim *deviation,
                                   char *err_reason,
                                   size_t err_len) {
    if (!ledger || !deviation) {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "null ledger or deviation");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    if (deviation->theme_id[0] == '\0') {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "missing mandatory field: theme_id");
        return CBM_REFUSAL_CLAIM_INVALID;
    }
    if (deviation->theme_rule_node_ref[0] == '\0') {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "missing mandatory field: theme_rule_node_ref");
        return CBM_REFUSAL_CLAIM_INVALID;
    }
    if (deviation->reason[0] == '\0') {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "missing mandatory field: reason");
        cbm_refusal_emit(CBM_REFUSAL_CLAIM_INVALID, "deviation_ledger", "missing mandatory field: reason");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    if (ledger->deviation_count >= CBM_DEVIATION_CAP) {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "deviation ledger capacity exceeded");
        return CBM_REFUSAL_SCOPE_EXCEEDED;
    }

    ledger->deviations[ledger->deviation_count] = *deviation;
    ledger->deviations[ledger->deviation_count].created_at = (uint64_t)time(NULL);
    ledger->deviation_count++;

    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_deviation_query_active(const CbmBindingLedger *ledger,
                                          const char *theme_id,
                                          CbmDeviationClaim *out_devs,
                                          size_t max_devs,
                                          size_t *out_count) {
    if (!ledger || !out_count) return CBM_REFUSAL_CLAIM_INVALID;

    size_t matched = 0;
    for (size_t i = 0; i < ledger->deviation_count && matched < max_devs; i++) {
        if (ledger->deviations[i].status == CBM_DEVIATION_ACTIVE) {
            if (!theme_id || strcmp(ledger->deviations[i].theme_id, theme_id) == 0) {
                if (out_devs) out_devs[matched] = ledger->deviations[i];
                matched++;
            }
        }
    }
    *out_count = matched;
    return CBM_REFUSAL_OK;
}
