/*
 * test_union_contract.c — Scope A02 acceptance criteria.
 */
#include "test_framework.h"
#include "../src/union/union_contract.h"

#include <string.h>

static CbmSkillContract make_contract(const char *identity, const char *territory) {
    CbmSkillContract c;
    memset(&c, 0, sizeof(c));
    snprintf(c.identity, sizeof(c.identity), "%s", identity);
    snprintf(c.territory[0], sizeof(c.territory[0]), "%s", territory);
    c.territory_count = 1;
    c.effect_class = CBM_EFFECT_IDEMPOTENT;
    c.acknowledged_refusals_mask =
        (1u << CBM_REFUSAL_ANCHOR_NOT_FOUND) | (1u << CBM_REFUSAL_STALE_BASE);
    c.requires_trace = true;
    c.requires_exclusion_summary = true;
    snprintf(c.provenance, sizeof(c.provenance), "test:initial");
    return c;
}

/* AC2: a valid contract is admitted and referenced. */
TEST(test_contract_valid_admits) {
    CbmContractRegistry registry;
    cbm_contract_registry_init(&registry);

    CbmSkillContract c = make_contract("graph_grounding", "context_read");
    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    ASSERT_EQ(cbm_contract_registry_put(&registry, &c, err, sizeof(err)), CBM_CONTRACT_OK);

    const CbmSkillContract *got = cbm_contract_registry_get(&registry, "graph_grounding");
    ASSERT_NOT_NULL(got);
    ASSERT_STR_EQ(got->territory[0], "context_read");
    ASSERT_EQ(got->effect_class, CBM_EFFECT_IDEMPOTENT);
    PASS();
}

/* AC1: missing/invalid field -> refusal naming the exact field. */
TEST(test_contract_invalid_field_named) {
    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    CbmSkillContract c = make_contract("ok_identity", "ok_territory");

    /* Missing identity. */
    CbmSkillContract bad = c;
    bad.identity[0] = '\0';
    ASSERT_EQ(cbm_contract_validate(&bad, err, sizeof(err)), CBM_CONTRACT_ERR_INVALID);
    ASSERT_STR_EQ(err, "identity");

    /* Missing territory. */
    bad = c;
    bad.territory_count = 0;
    ASSERT_EQ(cbm_contract_validate(&bad, err, sizeof(err)), CBM_CONTRACT_ERR_INVALID);
    ASSERT_STR_EQ(err, "territory");

    /* Unclassified effect (AC5): the contract must classify everything. */
    bad = c;
    bad.effect_class = CBM_EFFECT_UNCLASSIFIED;
    ASSERT_EQ(cbm_contract_validate(&bad, err, sizeof(err)), CBM_CONTRACT_ERR_INVALID);
    ASSERT_STR_EQ(err, "effect_class");

    /* Empty refusal matrix: a skill acknowledging nothing cannot join. */
    bad = c;
    bad.acknowledged_refusals_mask = 0;
    ASSERT_EQ(cbm_contract_validate(&bad, err, sizeof(err)), CBM_CONTRACT_ERR_INVALID);
    ASSERT_STR_EQ(err, "refusal_matrix");

    /* Invalid identity charset. */
    bad = c;
    snprintf(bad.identity, sizeof(bad.identity), "Not A Valid Identity");
    ASSERT_EQ(cbm_contract_validate(&bad, err, sizeof(err)), CBM_CONTRACT_ERR_INVALID);
    ASSERT_STR_EQ(err, "identity");

    /* NULL contract. */
    ASSERT_EQ(cbm_contract_validate(NULL, err, sizeof(err)), CBM_CONTRACT_ERR_NULL);
    ASSERT_STR_EQ(err, "contract");
    PASS();
}

/* A duplicate identity is refused: the registry never edits (supersession is
 * a later scope; identity is stable). */
TEST(test_contract_duplicate_refused) {
    CbmContractRegistry registry;
    cbm_contract_registry_init(&registry);

    CbmSkillContract a = make_contract("same_id", "territory_a");
    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    ASSERT_EQ(cbm_contract_registry_put(&registry, &a, err, sizeof(err)), CBM_CONTRACT_OK);

    CbmSkillContract b = make_contract("same_id", "territory_b");
    ASSERT_EQ(cbm_contract_registry_put(&registry, &b, err, sizeof(err)),
              CBM_CONTRACT_ERR_EXISTS);
    ASSERT_STR_EQ(err, "identity");

    /* The first registration is untouched: no silent merge, no edit. */
    const CbmSkillContract *got = cbm_contract_registry_get(&registry, "same_id");
    ASSERT_NOT_NULL(got);
    ASSERT_STR_EQ(got->territory[0], "territory_a");
    PASS();
}

/* AC3: unknown identity -> NULL (restricted mode is the caller's consequence). */
TEST(test_contract_get_unknown) {
    CbmContractRegistry registry;
    cbm_contract_registry_init(&registry);
    ASSERT_NULL(cbm_contract_registry_get(&registry, "nobody"));
    ASSERT_NULL(cbm_contract_registry_get(&registry, NULL));
    ASSERT_NULL(cbm_contract_registry_get(NULL, "anyone"));
    PASS();
}

/* AC4: territory overlap is reported, never silently merged. */
TEST(test_contract_overlap_reported) {
    CbmContractRegistry registry;
    cbm_contract_registry_init(&registry);

    CbmSkillContract a = make_contract("adversarial_traverser", "contest");
    CbmSkillContract b = make_contract("union_tracer", "trace");
    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    ASSERT_EQ(cbm_contract_registry_put(&registry, &a, err, sizeof(err)), CBM_CONTRACT_OK);
    ASSERT_EQ(cbm_contract_registry_put(&registry, &b, err, sizeof(err)), CBM_CONTRACT_OK);

    /* Candidate overlapping one existing contract. */
    CbmSkillContract cand = make_contract("constellation_individuator", "contest");
    char overlaps[4][CBM_CONTRACT_ID_MAX];
    size_t n = cbm_contract_registry_overlap(&registry, &cand, overlaps, 4);
    ASSERT_EQ(n, 1);
    ASSERT_STR_EQ(overlaps[0], "adversarial_traverser");

    /* Candidate with two territories overlapping two contracts. */
    CbmSkillContract wide = make_contract("wide_skill", "trace");
    snprintf(wide.territory[1], sizeof(wide.territory[1]), "contest");
    wide.territory_count = 2;
    n = cbm_contract_registry_overlap(&registry, &wide, overlaps, 4);
    ASSERT_EQ(n, 2);

    /* No overlap. */
    CbmSkillContract clean = make_contract("clean_skill", "fresh_territory");
    n = cbm_contract_registry_overlap(&registry, &clean, overlaps, 4);
    ASSERT_EQ(n, 0);
    PASS();
}

/* Registry capacity is bounded; full is an honest refusal. */
TEST(test_contract_registry_full) {
    CbmContractRegistry registry;
    cbm_contract_registry_init(&registry);

    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    for (size_t i = 0; i < CBM_CONTRACT_REGISTRY_CAP; i++) {
        char id[CBM_CONTRACT_ID_MAX];
        snprintf(id, sizeof(id), "skill_%03zu", i);
        CbmSkillContract c = make_contract(id, "territory_x");
        ASSERT_EQ(cbm_contract_registry_put(&registry, &c, err, sizeof(err)), CBM_CONTRACT_OK);
    }
    ASSERT_EQ(registry.count, CBM_CONTRACT_REGISTRY_CAP);

    CbmSkillContract overflow = make_contract("skill_overflow", "territory_x");
    ASSERT_EQ(cbm_contract_registry_put(&registry, &overflow, err, sizeof(err)),
              CBM_CONTRACT_ERR_FULL);
    PASS();
}

/* Effect-class naming is closed (it feeds the A05 gateway later). */
TEST(test_effect_class_strings) {
    ASSERT_STR_EQ(cbm_effect_class_string(CBM_EFFECT_IDEMPOTENT), "IDEMPOTENT");
    ASSERT_STR_EQ(cbm_effect_class_string(CBM_EFFECT_COMPENSABLE), "COMPENSABLE");
    ASSERT_STR_EQ(cbm_effect_class_string(CBM_EFFECT_IRREVERSIBLE), "IRREVERSIBLE");
    ASSERT_STR_EQ(cbm_effect_class_string(CBM_EFFECT_UNCLASSIFIED), "UNCLASSIFIED");
    PASS();
}

SUITE(union_contract) {
    RUN_TEST(test_contract_valid_admits);
    RUN_TEST(test_contract_invalid_field_named);
    RUN_TEST(test_contract_duplicate_refused);
    RUN_TEST(test_contract_get_unknown);
    RUN_TEST(test_contract_overlap_reported);
    RUN_TEST(test_contract_registry_full);
    RUN_TEST(test_effect_class_strings);
}
