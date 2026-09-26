/*
 * test_union_theme_drift.c — Scope D05 acceptance criteria.
 *
 * Verified by host log / return value, never by narrator self-report.
 */
#include "test_framework.h"
#include "../src/union/union_theme_drift.h"
#include "../src/union/union_theme_registry.h"
#include "../src/union/union_binding.h"
#include "../src/union/union_refusal.h"

#include <string.h>

/* AC1: Advancing theme version propagates drift notice to bound projects */
TEST(test_theme_version_bump_emits_drift_notice) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmThemeEntry theme;
    memset(&theme, 0, sizeof(theme));
    strncpy(theme.theme_id, "@inst/clean-arch", sizeof(theme.theme_id) - 1);
    strncpy(theme.namespace, "institutional", sizeof(theme.namespace) - 1);
    strncpy(theme.curator, "lead_arch", sizeof(theme.curator) - 1);
    strncpy(theme.version, "1.0.0", sizeof(theme.version) - 1);
    theme.status = CBM_THEME_ACTIVE;
    ASSERT_EQ(cbm_theme_registry_register(&reg, &theme, NULL, 0), CBM_REFUSAL_OK);

    CbmBindingLedger ledger;
    cbm_binding_ledger_init(&ledger);

    CbmBindingClaim binding;
    memset(&binding, 0, sizeof(binding));
    strncpy(binding.claim_id, "bind_01", sizeof(binding.claim_id) - 1);
    strncpy(binding.theme_id, "@inst/clean-arch", sizeof(binding.theme_id) - 1);
    strncpy(binding.pinned_version, "1.0.0", sizeof(binding.pinned_version) - 1);
    binding.mode = CBM_BINDING_NORMATIVE;
    strncpy(binding.validated_by, "human_operator", sizeof(binding.validated_by) - 1);
    binding.status = CBM_BINDING_ACTIVE;
    ASSERT_EQ(cbm_binding_validate_and_admit(&ledger, &binding, NULL, 0), CBM_REFUSAL_OK);

    /* Advance theme version to 2.0.0 */
    CbmDriftNotice notices[4];
    size_t notice_count = 0;
    CbmRefusalCode code = cbm_theme_publish_version(&reg, "@inst/clean-arch", "2.0.0",
                                                   &ledger, notices, 4, &notice_count);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_EQ(notice_count, 1);
    ASSERT_STR_EQ(notices[0].theme_id, "@inst/clean-arch");
    ASSERT_STR_EQ(notices[0].old_version, "1.0.0");
    ASSERT_STR_EQ(notices[0].new_version, "2.0.0");
    ASSERT_STR_EQ(notices[0].affected_claim_id, "bind_01");

    /* Binding status transitioned to DRIFT_PENDING, but pinned version remains unchanged */
    CbmBindingClaim out_binding;
    ASSERT_EQ(cbm_binding_lookup(&ledger, "@inst/clean-arch", &out_binding), CBM_REFUSAL_OK);
    ASSERT_EQ(out_binding.status, CBM_BINDING_DRIFT_PENDING);
    ASSERT_STR_EQ(out_binding.pinned_version, "1.0.0"); /* strictly not auto-bumped! */

    PASS();
}

/* AC2: Orphan binding detected when theme is absent or deprecated */
TEST(test_query_orphan_bindings_ecg) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    /* Register theme as DEPRECATED */
    CbmThemeEntry theme;
    memset(&theme, 0, sizeof(theme));
    strncpy(theme.theme_id, "@inst/old-arch", sizeof(theme.theme_id) - 1);
    strncpy(theme.namespace, "institutional", sizeof(theme.namespace) - 1);
    strncpy(theme.curator, "lead_arch", sizeof(theme.curator) - 1);
    strncpy(theme.version, "0.9.0", sizeof(theme.version) - 1);
    theme.status = CBM_THEME_DEPRECATED;
    ASSERT_EQ(cbm_theme_registry_register(&reg, &theme, NULL, 0), CBM_REFUSAL_OK);

    CbmBindingLedger ledger;
    cbm_binding_ledger_init(&ledger);

    /* Project bound to deprecated theme */
    CbmBindingClaim binding1;
    memset(&binding1, 0, sizeof(binding1));
    strncpy(binding1.claim_id, "bind_dep", sizeof(binding1.claim_id) - 1);
    strncpy(binding1.theme_id, "@inst/old-arch", sizeof(binding1.theme_id) - 1);
    strncpy(binding1.pinned_version, "0.9.0", sizeof(binding1.pinned_version) - 1);
    binding1.mode = CBM_BINDING_NORMATIVE;
    strncpy(binding1.validated_by, "human_operator", sizeof(binding1.validated_by) - 1);
    binding1.status = CBM_BINDING_ACTIVE;
    ASSERT_EQ(cbm_binding_validate_and_admit(&ledger, &binding1, NULL, 0), CBM_REFUSAL_OK);

    /* Project bound to completely missing theme */
    CbmBindingClaim binding2;
    memset(&binding2, 0, sizeof(binding2));
    strncpy(binding2.claim_id, "bind_ghost", sizeof(binding2.claim_id) - 1);
    strncpy(binding2.theme_id, "@inst/ghost-theme", sizeof(binding2.theme_id) - 1);
    strncpy(binding2.pinned_version, "1.0.0", sizeof(binding2.pinned_version) - 1);
    binding2.mode = CBM_BINDING_NORMATIVE;
    strncpy(binding2.validated_by, "human_operator", sizeof(binding2.validated_by) - 1);
    binding2.status = CBM_BINDING_ACTIVE;
    ASSERT_EQ(cbm_binding_validate_and_admit(&ledger, &binding2, NULL, 0), CBM_REFUSAL_OK);

    CbmOrphanBinding orphans[4];
    size_t orphan_count = 0;
    CbmRefusalCode code = cbm_query_orphan_bindings(&reg, &ledger, orphans, 4, &orphan_count);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_EQ(orphan_count, 2);

    PASS();
}

/* AC3: Query unreconciled deviations */
TEST(test_query_unreconciled_deviations) {
    CbmBindingLedger ledger;
    cbm_binding_ledger_init(&ledger);

    CbmDeviationClaim dev;
    memset(&dev, 0, sizeof(dev));
    strncpy(dev.claim_id, "dev_01", sizeof(dev.claim_id) - 1);
    strncpy(dev.theme_id, "@inst/clean-arch", sizeof(dev.theme_id) - 1);
    strncpy(dev.theme_rule_node_ref, "rules/no-raw-sql", sizeof(dev.theme_rule_node_ref) - 1);
    strncpy(dev.reason, "raw query needed for complex spatial aggregation", sizeof(dev.reason) - 1);
    dev.status = CBM_DEVIATION_ACTIVE;
    ASSERT_EQ(cbm_deviation_admit(&ledger, &dev, NULL, 0), CBM_REFUSAL_OK);

    CbmDeviationClaim results[4];
    size_t count = 0;
    CbmRefusalCode code = cbm_query_unreconciled_deviations(&ledger, results, 4, &count);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_EQ(count, 1);
    ASSERT_STR_EQ(results[0].claim_id, "dev_01");

    PASS();
}

SUITE(union_theme_drift) {
    RUN_TEST(test_theme_version_bump_emits_drift_notice);
    RUN_TEST(test_query_orphan_bindings_ecg);
    RUN_TEST(test_query_unreconciled_deviations);
}
