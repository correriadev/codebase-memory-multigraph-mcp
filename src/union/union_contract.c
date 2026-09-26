/*
 * union_contract.c — Skill Contract Registry (Scope A02).
 */
#include "union_contract.h"

#include <stdio.h>
#include <string.h>

const char *cbm_effect_class_string(CbmEffectClass cls) {
    switch (cls) {
    case CBM_EFFECT_IDEMPOTENT: return "IDEMPOTENT";
    case CBM_EFFECT_COMPENSABLE: return "COMPENSABLE";
    case CBM_EFFECT_IRREVERSIBLE: return "IRREVERSIBLE";
    case CBM_EFFECT_UNCLASSIFIED: return "UNCLASSIFIED";
    }
    return "UNCLASSIFIED";
}

void cbm_contract_registry_init(CbmContractRegistry *registry) {
    if (!registry) return;
    memset(registry, 0, sizeof(*registry));
}

static bool valid_identity(const char *s) {
    /* [a-z0-9][a-z0-9._-]* — a persona has a stable, machine-friendly name. */
    if (!s || !s[0]) return false;
    if (s[0] == '.' || s[0] == '-' || s[0] == '_') return false;
    for (const char *p = s; *p; p++) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '-' || c == '_')) {
            return false;
        }
    }
    return true;
}

static bool territory_empty(const char *s) {
    return !s || s[0] == '\0';
}

CbmContractResult cbm_contract_validate(const CbmSkillContract *contract,
                                       char *out_error, size_t err_sz) {
    if (!contract) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "contract");
        return CBM_CONTRACT_ERR_NULL;
    }

    if (!valid_identity(contract->identity)) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "identity");
        return CBM_CONTRACT_ERR_INVALID;
    }

    if (contract->territory_count == 0 || contract->territory_count > CBM_CONTRACT_TERRITORY_MAX) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "territory");
        return CBM_CONTRACT_ERR_INVALID;
    }
    for (size_t i = 0; i < contract->territory_count; i++) {
        if (territory_empty(contract->territory[i])) {
            if (out_error && err_sz > 0) snprintf(out_error, err_sz, "territory");
            return CBM_CONTRACT_ERR_INVALID;
        }
    }

    /* An unclassified action is treated as irreversible and blocked (A05) —
     * a contract that declares UNCLASSIFIED is refused at the door. */
    if (contract->effect_class == CBM_EFFECT_UNCLASSIFIED) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "effect_class");
        return CBM_CONTRACT_ERR_INVALID;
    }

    /* The refusal matrix is the skill's acknowledged curriculum (B04): a
     * skill that acknowledges nothing cannot be part of the union. */
    if (contract->acknowledged_refusals_mask == 0) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "refusal_matrix");
        return CBM_CONTRACT_ERR_INVALID;
    }

    return CBM_CONTRACT_OK;
}

CbmContractResult cbm_contract_registry_put(CbmContractRegistry *registry,
                                            const CbmSkillContract *contract,
                                            char *out_error, size_t err_sz) {
    CbmContractResult vr = cbm_contract_validate(contract, out_error, err_sz);
    if (vr != CBM_CONTRACT_OK) return vr;

    if (!registry) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "registry");
        return CBM_CONTRACT_ERR_NULL;
    }

    for (size_t i = 0; i < registry->count; i++) {
        if (strcmp(registry->contracts[i].identity, contract->identity) == 0) {
            if (out_error && err_sz > 0) snprintf(out_error, err_sz, "identity");
            return CBM_CONTRACT_ERR_EXISTS;
        }
    }

    if (registry->count >= CBM_CONTRACT_REGISTRY_CAP) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "registry");
        return CBM_CONTRACT_ERR_FULL;
    }

    /* Copy by value: the registry owns its storage; the caller's buffers may
     * go out of scope. */
    registry->contracts[registry->count++] = *contract;
    return CBM_CONTRACT_OK;
}

const CbmSkillContract *cbm_contract_registry_get(const CbmContractRegistry *registry,
                                                  const char *identity) {
    if (!registry || !identity) return NULL;
    for (size_t i = 0; i < registry->count; i++) {
        if (strcmp(registry->contracts[i].identity, identity) == 0) {
            return &registry->contracts[i];
        }
    }
    return NULL;
}

size_t cbm_contract_registry_overlap(const CbmContractRegistry *registry,
                                     const CbmSkillContract *candidate,
                                     char out_ids[][CBM_CONTRACT_ID_MAX],
                                     size_t max_out) {
    if (!registry || !candidate) return 0;
    size_t found = 0;
    for (size_t i = 0; i < registry->count; i++) {
        bool overlaps = false;
        for (size_t a = 0; a < registry->contracts[i].territory_count && !overlaps; a++) {
            for (size_t b = 0; b < candidate->territory_count; b++) {
                if (strcmp(registry->contracts[i].territory[a], candidate->territory[b]) == 0) {
                    overlaps = true;
                    break;
                }
            }
        }
        if (overlaps) {
            if (out_ids && found < max_out) {
                snprintf(out_ids[found], CBM_CONTRACT_ID_MAX, "%s",
                         registry->contracts[i].identity);
            }
            found++;
        }
    }
    return found;
}
