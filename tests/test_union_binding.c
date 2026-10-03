/*
 * test_union_binding.c — Scope D02 acceptance criteria.
 *
 * Verified by host log / return value, never by narrator self-report.
 */
#include "test_framework.h"
#include "../src/union/union_binding.h"
#include "../src/union/union_refusal.h"
#include "../src/foundation/compat.h"
#include "../src/foundation/compat_fs.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

static void populate_valid_binding(CbmBindingClaim *binding) {
    memset(binding, 0, sizeof(*binding));
    strncpy(binding->claim_id, "bind_01", sizeof(binding->claim_id) - 1);
    strncpy(binding->theme_id, "@inst/clean-arch", sizeof(binding->theme_id) - 1);
    strncpy(binding->pinned_version, "2.1.0", sizeof(binding->pinned_version) - 1);
    binding->mode = CBM_BINDING_NORMATIVE;
    strncpy(binding->binding_scope, "src/domain", sizeof(binding->binding_scope) - 1);
    strncpy(binding->validated_by, "human_operator", sizeof(binding->validated_by) - 1);
    binding->status = CBM_BINDING_ACTIVE;
}

static void populate_valid_deviation(CbmDeviationClaim *dev) {
    memset(dev, 0, sizeof(*dev));
    strncpy(dev->claim_id, "dev_01", sizeof(dev->claim_id) - 1);
    strncpy(dev->theme_id, "@inst/clean-arch", sizeof(dev->theme_id) - 1);
    strncpy(dev->theme_rule_node_ref, "rules/no-direct-db-in-controller", sizeof(dev->theme_rule_node_ref) - 1);
    strncpy(dev->reason, "Temporary migration bypass for legacy reporting module", sizeof(dev->reason) - 1);
    strncpy(dev->affected_scope, "src/legacy/reporter.c", sizeof(dev->affected_scope) - 1);
    strncpy(dev->validated_by, "human_operator", sizeof(dev->validated_by) - 1);
    dev->status = CBM_DEVIATION_ACTIVE;
}

/* AC1: Agent self-validation of a binding is refused */
TEST(test_binding_agent_self_validation_refused) {
    CbmBindingLedger ledger;
    cbm_binding_ledger_init(&ledger);

    CbmBindingClaim binding;
    populate_valid_binding(&binding);
    strncpy(binding.validated_by, "agent_worker", sizeof(binding.validated_by) - 1);

    char reason[128];
    CbmRefusalCode code = cbm_binding_validate_and_admit(&ledger, &binding, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_BINDING_SELF_VALIDATED);
    ASSERT_TRUE(strstr(reason, "operator") != NULL);

    PASS();
}

/* AC2: Valid operator-validated normative binding succeeds and is immutable in version */
TEST(test_binding_operator_normative_admitted) {
    CbmBindingLedger ledger;
    cbm_binding_ledger_init(&ledger);

    CbmBindingClaim binding;
    populate_valid_binding(&binding);

    CbmRefusalCode code = cbm_binding_validate_and_admit(&ledger, &binding, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    CbmBindingClaim out_binding;
    code = cbm_binding_lookup(&ledger, "@inst/clean-arch", &out_binding);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_STR_EQ(out_binding.pinned_version, "2.1.0");
    ASSERT_EQ(out_binding.mode, CBM_BINDING_NORMATIVE);
    ASSERT_STR_EQ(out_binding.validated_by, "human_operator");

    PASS();
}

/* AC3: Consulted (PODE) binding does not mandate operator validation */
TEST(test_binding_consulted_mode_optional_operator) {
    CbmBindingLedger ledger;
    cbm_binding_ledger_init(&ledger);

    CbmBindingClaim binding;
    populate_valid_binding(&binding);
    binding.mode = CBM_BINDING_CONSULTED;
    binding.validated_by[0] = '\0'; /* empty operator */

    CbmRefusalCode code = cbm_binding_validate_and_admit(&ledger, &binding, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    CbmBindingClaim out_binding;
    ASSERT_EQ(cbm_binding_lookup(&ledger, "@inst/clean-arch", &out_binding), CBM_REFUSAL_OK);
    ASSERT_EQ(out_binding.mode, CBM_BINDING_CONSULTED);

    PASS();
}

/* AC4: Intentional deviation recorded as visible scar */
TEST(test_deviation_claim_ledger_scar) {
    CbmBindingLedger ledger;
    cbm_binding_ledger_init(&ledger);

    CbmDeviationClaim dev;
    populate_valid_deviation(&dev);

    CbmRefusalCode code = cbm_deviation_admit(&ledger, &dev, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    CbmDeviationClaim out_devs[4];
    size_t count = 0;
    code = cbm_deviation_query_active(&ledger, "@inst/clean-arch", out_devs, 4, &count);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_EQ(count, 1);
    ASSERT_STR_EQ(out_devs[0].theme_rule_node_ref, "rules/no-direct-db-in-controller");
    ASSERT_STR_EQ(out_devs[0].reason, "Temporary migration bypass for legacy reporting module");
    ASSERT_EQ(out_devs[0].status, CBM_DEVIATION_ACTIVE);

    PASS();
}

/* AC5: Deviation missing reason is refused with CLAIM_INVALID */
TEST(test_deviation_missing_reason_refused) {
    CbmBindingLedger ledger;
    cbm_binding_ledger_init(&ledger);

    CbmDeviationClaim dev;
    populate_valid_deviation(&dev);
    dev.reason[0] = '\0'; /* missing rationale */

    char err[128];
    CbmRefusalCode code = cbm_deviation_admit(&ledger, &dev, err, sizeof(err));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(err, "reason") != NULL);

    PASS();
}

TEST(test_binding_and_deviation_ledger_survives_reopen) {
    char path[CBM_BINDING_STORE_PATH_MAX];
    char tmp[CBM_BINDING_STORE_PATH_MAX + 8];
    int n = snprintf(path,sizeof(path),"%s/cbm-binding-test-%llu.json",cbm_tmpdir(),
                     (unsigned long long)time(NULL));
    ASSERT_TRUE(n > 0 && (size_t)n < sizeof(path));
    snprintf(tmp,sizeof(tmp),"%s.tmp",path);
    (void)cbm_unlink(path); (void)cbm_unlink(tmp);
    CbmBindingLedger writer;
    ASSERT_TRUE(cbm_binding_ledger_open(&writer,path));
    CbmBindingClaim binding; populate_valid_binding(&binding);
    ASSERT_EQ(cbm_binding_validate_and_admit(&writer,&binding,NULL,0),CBM_REFUSAL_OK);
    CbmDeviationClaim deviation; populate_valid_deviation(&deviation);
    ASSERT_EQ(cbm_deviation_admit(&writer,&deviation,NULL,0),CBM_REFUSAL_OK);
    CbmBindingLedger reader;
    ASSERT_TRUE(cbm_binding_ledger_open(&reader,path));
    CbmBindingClaim recovered;
    ASSERT_EQ(cbm_binding_lookup(&reader,binding.theme_id,&recovered),CBM_REFUSAL_OK);
    ASSERT_STR_EQ(recovered.pinned_version,binding.pinned_version);
    ASSERT_STR_EQ(recovered.validated_by,binding.validated_by);
    CbmDeviationClaim deviations[2]; size_t count=0;
    ASSERT_EQ(cbm_deviation_query_active(&reader,binding.theme_id,deviations,2,&count),CBM_REFUSAL_OK);
    ASSERT_EQ(count,1);
    ASSERT_STR_EQ(deviations[0].reason,deviation.reason);
    ASSERT_EQ(cbm_unlink(path),0); (void)cbm_unlink(tmp);
    PASS();
}

SUITE(union_binding) {
    RUN_TEST(test_binding_agent_self_validation_refused);
    RUN_TEST(test_binding_operator_normative_admitted);
    RUN_TEST(test_binding_consulted_mode_optional_operator);
    RUN_TEST(test_deviation_claim_ledger_scar);
    RUN_TEST(test_deviation_missing_reason_refused);
    RUN_TEST(test_binding_and_deviation_ledger_survives_reopen);
}
