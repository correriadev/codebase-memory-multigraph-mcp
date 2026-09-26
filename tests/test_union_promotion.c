/*
 * test_union_promotion.c — Scope A07 and A08 acceptance criteria.
 */
#include "test_framework.h"
#include "../src/union/union_promotion.h"

#include <string.h>

static CbmPromotionProposal make_valid_proposal(const char *submitter) {
    CbmPromotionProposal p;
    memset(&p, 0, sizeof(p));
    snprintf(p.submitter_identity, sizeof(p.submitter_identity), "%s", submitter);
    snprintf(p.source_horizon_id, sizeof(p.source_horizon_id), "h_child");
    snprintf(p.target_horizon_id, sizeof(p.target_horizon_id), "h_parent");
    snprintf(p.expected_dag_parent, sizeof(p.expected_dag_parent), "h_parent");
    p.candidate_count = 5;
    p.promoted_count = 3;
    p.exclusions.declared = true;
    p.exclusions.counts[CBM_EXCLUSION_ANCHOR_FAILED] = 1;
    p.exclusions.counts[CBM_EXCLUSION_OUT_OF_SCOPE] = 1;
    return p;
}

/* AC1: undeclared exclusion is refused */
TEST(test_promotion_undeclared_exclusion_refused) {
    CbmPromotionProposal p = make_valid_proposal("skill_dev");
    p.exclusions.declared = false;

    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    char reason[128] = {0};

    CbmPromotionOutcome res = cbm_promotion_evaluate(&p, NULL, &refusal, reason, sizeof(reason));
    ASSERT_EQ(res, CBM_PROMOTION_REFUSED);
    ASSERT_EQ(refusal, CBM_REFUSAL_EXCLUSION_UNDECLARED);
    PASS();
}

/* AC3: empty promotion is first-class outcome */
TEST(test_promotion_empty_first_class) {
    CbmPromotionProposal p = make_valid_proposal("skill_dev");
    p.promoted_count = 0;
    p.exclusions.counts[CBM_EXCLUSION_OUT_OF_SCOPE] = 5;

    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    char reason[128] = {0};

    CbmPromotionOutcome res = cbm_promotion_evaluate(&p, NULL, &refusal, reason, sizeof(reason));
    ASSERT_EQ(res, CBM_PROMOTION_EMPTY);
    ASSERT_EQ(refusal, CBM_REFUSAL_OK);
    PASS();
}

/* AC4: horizon skipping is refused */
TEST(test_promotion_horizon_skip_refused) {
    CbmPromotionProposal p = make_valid_proposal("skill_dev");
    snprintf(p.target_horizon_id, sizeof(p.target_horizon_id), "h_grandparent");

    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    char reason[128] = {0};

    CbmPromotionOutcome res = cbm_promotion_evaluate(&p, NULL, &refusal, reason, sizeof(reason));
    ASSERT_EQ(res, CBM_PROMOTION_REFUSED);
    ASSERT_EQ(refusal, CBM_REFUSAL_HORIZON_SKIP);
    PASS();
}

/* AC5: assumption dropped is refused */
TEST(test_promotion_assumption_dropped_refused) {
    CbmPromotionProposal p = make_valid_proposal("skill_dev");
    snprintf(p.recorded_assumptions[0], sizeof(p.recorded_assumptions[0]), "assume_db_online");
    p.recorded_assumptions_count = 1;
    p.resolved_assumptions_count = 0; /* not resolved or carried */

    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    char reason[128] = {0};

    CbmPromotionOutcome res = cbm_promotion_evaluate(&p, NULL, &refusal, reason, sizeof(reason));
    ASSERT_EQ(res, CBM_PROMOTION_REFUSED);
    ASSERT_EQ(refusal, CBM_REFUSAL_ASSUMPTION_DROPPED);
    ASSERT_STR_EQ(reason, "assumption_dropped:assume_db_online");
    PASS();
}

/* Scope A08: Caller-Blind Invariance */
TEST(test_promotion_caller_blind_invariance) {
    CbmPromotionProposal p1 = make_valid_proposal("senior_architect");
    CbmPromotionProposal p2 = make_valid_proposal("anonymous_agent");
    CbmPromotionProposal p3 = make_valid_proposal("hostile_adversary");

    CbmRefusalCode r1 = CBM_REFUSAL_OK, r2 = CBM_REFUSAL_OK, r3 = CBM_REFUSAL_OK;
    char re1[128] = {0}, re2[128] = {0}, re3[128] = {0};

    CbmPromotionOutcome out1 = cbm_promotion_evaluate(&p1, NULL, &r1, re1, sizeof(re1));
    CbmPromotionOutcome out2 = cbm_promotion_evaluate(&p2, NULL, &r2, re2, sizeof(re2));
    CbmPromotionOutcome out3 = cbm_promotion_evaluate(&p3, NULL, &r3, re3, sizeof(re3));

    /* Invariance: all outcomes and refusals are strictly identical across identities */
    ASSERT_EQ(out1, CBM_PROMOTION_ADMITTED);
    ASSERT_EQ(out1, out2);
    ASSERT_EQ(out2, out3);
    ASSERT_EQ(r1, r2);
    ASSERT_EQ(r2, r3);
    PASS();
}

SUITE(union_promotion) {
    RUN_TEST(test_promotion_undeclared_exclusion_refused);
    RUN_TEST(test_promotion_empty_first_class);
    RUN_TEST(test_promotion_horizon_skip_refused);
    RUN_TEST(test_promotion_assumption_dropped_refused);
    RUN_TEST(test_promotion_caller_blind_invariance);
}
