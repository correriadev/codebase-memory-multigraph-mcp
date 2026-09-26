/*
 * union_theme_drift.h — Theme Drift Propagation & Orphan Binding ECG Queries (Scope D05).
 *
 * See docs/PRD/novos-paradgimas/specs/knowledge-base/SCOPE-D05-theme-drift-propagation.md
 */
#ifndef CBM_UNION_THEME_DRIFT_H
#define CBM_UNION_THEME_DRIFT_H

#include "union_refusal.h"
#include "union_theme_registry.h"
#include "union_binding.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char theme_id[CBM_THEME_ID_MAX];
    char old_version[CBM_THEME_VER_MAX];
    char new_version[CBM_THEME_VER_MAX];
    char affected_claim_id[CBM_BINDING_ID_MAX];
    uint64_t emitted_at;
} CbmDriftNotice;

typedef struct {
    CbmBindingClaim binding;
    char reason[128];
} CbmOrphanBinding;

CbmRefusalCode cbm_theme_publish_version(CbmThemeRegistry *reg,
                                        const char *theme_id,
                                        const char *new_version,
                                        CbmBindingLedger *ledger,
                                        CbmDriftNotice *out_notices,
                                        size_t max_notices,
                                        size_t *out_count);

CbmRefusalCode cbm_query_orphan_bindings(const CbmThemeRegistry *reg,
                                        const CbmBindingLedger *ledger,
                                        CbmOrphanBinding *out_orphans,
                                        size_t max_orphans,
                                        size_t *out_count);

CbmRefusalCode cbm_query_unreconciled_deviations(const CbmBindingLedger *ledger,
                                                CbmDeviationClaim *out_devs,
                                                size_t max_devs,
                                                size_t *out_count);

#endif /* CBM_UNION_THEME_DRIFT_H */
