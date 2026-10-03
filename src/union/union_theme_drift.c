/*
 * union_theme_drift.c — Theme Drift Propagation & Orphan Binding ECG Queries (Scope D05).
 */
#include "union_theme_drift.h"
#include "../foundation/log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

CbmRefusalCode cbm_theme_publish_version(CbmThemeRegistry *reg,
                                        const char *theme_id,
                                        const char *new_version,
                                        CbmBindingLedger *ledger,
                                        CbmDriftNotice *out_notices,
                                        size_t max_notices,
                                        size_t *out_count) {
    if (!reg || !theme_id || !new_version) return CBM_REFUSAL_THEME_UNKNOWN;

    CbmThemeEntry current;
    CbmRefusalCode code = cbm_theme_registry_lookup(reg, theme_id, &current);
    if (code != CBM_REFUSAL_OK) return code;

    char old_ver[CBM_THEME_VER_MAX];
    strncpy(old_ver, current.version, sizeof(old_ver) - 1);
    old_ver[sizeof(old_ver) - 1] = '\0';

    /* Update theme version in registry */
    strncpy(current.version, new_version, sizeof(current.version) - 1);
    current.version[sizeof(current.version) - 1] = '\0';
    code = cbm_theme_registry_register(reg, &current, NULL, 0);
    if (code != CBM_REFUSAL_OK) {
        if (out_count) *out_count = 0;
        return code;
    }

    size_t count = 0;
    if (ledger) {
        for (size_t i = 0; i < ledger->binding_count; i++) {
            CbmBindingClaim *b = &ledger->bindings[i];
            if (strcmp(b->theme_id, theme_id) == 0) {
                if (strcmp(b->pinned_version, new_version) != 0) {
                    b->status = CBM_BINDING_DRIFT_PENDING;
                    b->updated_at = (uint64_t)time(NULL);

                    if (out_notices && count < max_notices) {
                        strncpy(out_notices[count].theme_id, theme_id, sizeof(out_notices[count].theme_id) - 1);
                        strncpy(out_notices[count].old_version, old_ver, sizeof(out_notices[count].old_version) - 1);
                        strncpy(out_notices[count].new_version, new_version, sizeof(out_notices[count].new_version) - 1);
                        strncpy(out_notices[count].affected_claim_id, b->claim_id, sizeof(out_notices[count].affected_claim_id) - 1);
                        out_notices[count].emitted_at = b->updated_at;
                        count++;
                    }
                }
            }
        }
    }

    if (out_count) *out_count = count;
    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_query_orphan_bindings(const CbmThemeRegistry *reg,
                                        const CbmBindingLedger *ledger,
                                        CbmOrphanBinding *out_orphans,
                                        size_t max_orphans,
                                        size_t *out_count) {
    if (!ledger || !out_count) return CBM_REFUSAL_CLAIM_INVALID;

    size_t count = 0;
    for (size_t i = 0; i < ledger->binding_count && count < max_orphans; i++) {
        const CbmBindingClaim *b = &ledger->bindings[i];
        CbmThemeEntry entry;
        CbmRefusalCode code = reg ? cbm_theme_registry_lookup(reg, b->theme_id, &entry) : CBM_REFUSAL_THEME_UNKNOWN;

        if (code == CBM_REFUSAL_THEME_UNKNOWN) {
            if (out_orphans) {
                out_orphans[count].binding = *b;
                snprintf(out_orphans[count].reason, sizeof(out_orphans[count].reason),
                         "theme %s not found in registry", b->theme_id);
            }
            count++;
        } else if (entry.status == CBM_THEME_DEPRECATED) {
            if (out_orphans) {
                out_orphans[count].binding = *b;
                snprintf(out_orphans[count].reason, sizeof(out_orphans[count].reason),
                         "theme %s is deprecated", b->theme_id);
            }
            count++;
        } else if (entry.status == CBM_THEME_ABSENT) {
            if (out_orphans) {
                out_orphans[count].binding = *b;
                snprintf(out_orphans[count].reason, sizeof(out_orphans[count].reason),
                         "theme %s is absent (unmaterialized)", b->theme_id);
            }
            count++;
        }
    }

    *out_count = count;
    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_query_unreconciled_deviations(const CbmBindingLedger *ledger,
                                                CbmDeviationClaim *out_devs,
                                                size_t max_devs,
                                                size_t *out_count) {
    if (!ledger || !out_count) return CBM_REFUSAL_CLAIM_INVALID;
    return cbm_deviation_query_active(ledger, NULL, out_devs, max_devs, out_count);
}
