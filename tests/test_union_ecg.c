/*
 * test_union_ecg.c — Scope C07 acceptance criteria (The ECG: Aging & Orphan Queries).
 */
#include "test_framework.h"
#include "../src/union/union_ecg.h"

#include <string.h>

static void populate_test_claim(CbmClaim *claim, const char *id,
                                const char *predicate, const char *consequence,
                                const char *validator, const char *proposer) {
    memset(claim, 0, sizeof(*claim));
    strncpy(claim->id, id, sizeof(claim->id) - 1);
    strncpy(claim->predicate, predicate, sizeof(claim->predicate) - 1);
    claim->type = CBM_CLAIM_DECISION;
    claim->status = EPISTEMIC_ACCEPTED;
    claim->realized_by_is_empty = true; /* ∅ unrealized */

    if (consequence) {
        strncpy(claim->consequence, consequence, sizeof(claim->consequence) - 1);
    }
    if (validator) {
        strncpy(claim->provenance.validated_by, validator, sizeof(claim->provenance.validated_by) - 1);
    }
    if (proposer) {
        strncpy(claim->provenance.proposed_by, proposer, sizeof(claim->provenance.proposed_by) - 1);
    }
}

/* AC1: Three admitted claims with ages 5d, 40d, 90d; query threshold 30d
 * -> exactly 40d and 90d claims return with consequence, status, validated_by. */
TEST(test_ecg_aging_intention_threshold) {
    CbmClaim claims[3];
    uint64_t ages[3] = {5, 40, 90};

    populate_test_claim(&claims[0], "claim_5d", "Fresh idea.", "Minor impact.", "operator", "agent");
    populate_test_claim(&claims[1], "claim_40d", "Medium aging idea.", "System desync if forgotten.", "operator", "agent");
    populate_test_claim(&claims[2], "claim_90d", "Old dying idea.", "Security vulnerability if unpatched.", "operator", "agent");

    CbmAgingIntentionResult results[4];
    size_t count = cbm_ecg_query_aging_intentions(claims, 3, ages, 30, NULL, results, 4);

    ASSERT_EQ(count, 2);
    ASSERT_STR_EQ(results[0].claim_id, "claim_40d");
    ASSERT_EQ(results[0].age_days, 40);
    ASSERT_STR_EQ(results[0].consequence, "System desync if forgotten.");
    ASSERT_EQ(results[0].status, EPISTEMIC_ACCEPTED);
    ASSERT_STR_EQ(results[0].validated_by, "operator");

    ASSERT_STR_EQ(results[1].claim_id, "claim_90d");
    ASSERT_EQ(results[1].age_days, 90);
    ASSERT_STR_EQ(results[1].consequence, "Security vulnerability if unpatched.");

    PASS();
}

/* AC2: Operator-validated intention and model-assumed intention are distinguishable in results
 * (never flattened into one number). */
TEST(test_ecg_distinguishes_operator_vs_model_intention) {
    CbmClaim claims[2];
    uint64_t ages[2] = {60, 60};

    populate_test_claim(&claims[0], "claim_op", "Operator decision.", "Breaks contract.", "operator", "agent");
    populate_test_claim(&claims[1], "claim_agent", "Model assumption.", "Performance issue.", "", "agent");

    CbmAgingIntentionResult results[4];
    size_t count = cbm_ecg_query_aging_intentions(claims, 2, ages, 30, NULL, results, 4);

    ASSERT_EQ(count, 2);
    ASSERT_STR_EQ(results[0].validated_by, "operator");
    ASSERT_STR_EQ(results[1].validated_by, "");

    /* Check owner filter for operator */
    count = cbm_ecg_query_aging_intentions(claims, 2, ages, 30, "operator", results, 4);
    ASSERT_EQ(count, 1);
    ASSERT_STR_EQ(results[0].claim_id, "claim_op");

    PASS();
}

/* AC3: Code claim realized by a doc claim -> absent from orphan query results. */
TEST(test_ecg_realized_code_is_not_orphan) {
    CbmCodeSymbolRegistry code_reg;
    cbm_code_registry_init(&code_reg);
    cbm_code_registry_add(&code_reg, "src/auth.c#validate_token");

    CbmDocEdgeStore edge_store;
    cbm_doc_edge_store_init(&edge_store);

    /* Record REALIZED_BY edge from doc claim to code */
    CbmDocEdge edge = {0};
    strncpy(edge.source, "claim_auth_spec", sizeof(edge.source) - 1);
    strncpy(edge.target, "src/auth.c#validate_token", sizeof(edge.target) - 1);
    edge.type = CBM_DOC_EDGE_REALIZED_BY;
    (void)cbm_doc_edge_add(&edge_store, NULL, &edge, NULL, 0);

    CbmOrphanRealizationResult results[4];
    size_t count = cbm_ecg_query_orphan_realizations(&code_reg, &edge_store, NULL, results, 4);

    ASSERT_EQ(count, 0);
    PASS();
}

/* AC4: Code claims without realizing doc claim -> return with refs, count is log-computable. */
TEST(test_ecg_orphan_realizations_returned) {
    CbmCodeSymbolRegistry code_reg;
    cbm_code_registry_init(&code_reg);
    cbm_code_registry_add(&code_reg, "src/billing.c#charge_card");
    cbm_code_registry_add(&code_reg, "src/billing.c#refund");

    CbmDocEdgeStore edge_store;
    cbm_doc_edge_store_init(&edge_store);
    /* Zero doc realization edges */

    CbmOrphanRealizationResult results[4];
    size_t count = cbm_ecg_query_orphan_realizations(&code_reg, &edge_store, NULL, results, 4);

    ASSERT_EQ(count, 2);
    ASSERT_STR_EQ(results[0].code_symbol, "src/billing.c#charge_card");
    ASSERT_STR_EQ(results[1].code_symbol, "src/billing.c#refund");

    PASS();
}

/* AC5: Determinism test: queries return identical results when executed twice. */
TEST(test_ecg_query_determinism) {
    CbmClaim claims[1];
    uint64_t ages[1] = {45};
    populate_test_claim(&claims[0], "claim_det", "Deterministic test.", "None.", "operator", "agent");

    CbmAgingIntentionResult r1[2], r2[2];
    size_t c1 = cbm_ecg_query_aging_intentions(claims, 1, ages, 10, NULL, r1, 2);
    size_t c2 = cbm_ecg_query_aging_intentions(claims, 1, ages, 10, NULL, r2, 2);

    ASSERT_EQ(c1, c2);
    ASSERT_STR_EQ(r1[0].claim_id, r2[0].claim_id);
    ASSERT_EQ(r1[0].age_days, r2[0].age_days);

    PASS();
}

/* AC6: Metrics emission: intention-debt and orphan counts by bucket are log-computable. */
TEST(test_ecg_metrics_computation) {
    CbmClaim claims[4];
    uint64_t ages[4] = {10, 45, 100, 50};

    populate_test_claim(&claims[0], "c1", "Fresh", "c1", "operator", "agent");
    populate_test_claim(&claims[1], "c2", "Aging op", "c2", "operator", "agent");
    populate_test_claim(&claims[2], "c3", "Critical op", "c3", "operator", "agent");
    populate_test_claim(&claims[3], "c4", "Aging model", "c4", "", "agent");

    CbmCodeSymbolRegistry code_reg;
    cbm_code_registry_init(&code_reg);
    cbm_code_registry_add(&code_reg, "src/orphan1.c");
    cbm_code_registry_add(&code_reg, "src/orphan2.c");

    CbmDocEdgeStore edge_store;
    cbm_doc_edge_store_init(&edge_store);

    CbmEcgMetrics metrics;
    cbm_ecg_compute_metrics(claims, 4, ages, &code_reg, &edge_store, &metrics);

    ASSERT_EQ(metrics.intention_debt_fresh, 1);
    ASSERT_EQ(metrics.intention_debt_aging, 2);
    ASSERT_EQ(metrics.intention_debt_critical, 1);
    ASSERT_EQ(metrics.intention_debt_operator_validated, 3);
    ASSERT_EQ(metrics.intention_debt_model_assumed, 1);
    ASSERT_EQ(metrics.orphan_realizations_count, 2);

    PASS();
}

SUITE(union_ecg) {
    RUN_TEST(test_ecg_aging_intention_threshold);
    RUN_TEST(test_ecg_distinguishes_operator_vs_model_intention);
    RUN_TEST(test_ecg_realized_code_is_not_orphan);
    RUN_TEST(test_ecg_orphan_realizations_returned);
    RUN_TEST(test_ecg_query_determinism);
    RUN_TEST(test_ecg_metrics_computation);
}
