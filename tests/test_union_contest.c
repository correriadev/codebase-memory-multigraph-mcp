/*
 * test_union_contest.c — Scope A10 acceptance criteria.
 */
#include "test_framework.h"
#include "../src/union/union_contest.h"

#include <string.h>

TEST(test_contest_evidence_required) {
    CbmContestRegistry reg;
    cbm_contest_registry_init(&reg);

    CbmContestation c;
    memset(&c, 0, sizeof(c));
    snprintf(c.contest_id, sizeof(c.contest_id), "cnt_1");
    snprintf(c.source_horizon, sizeof(c.source_horizon), "h_adv");
    snprintf(c.target_ref, sizeof(c.target_ref), "cbm://repo/src/api#login");
    c.severity = CBM_CONTEST_BLOCKING;
    c.evidence_count = 0; /* No evidence provided */

    char reason[128] = {0};
    CbmRefusalCode res = cbm_contest_submit(&reg, &c, reason, sizeof(reason));
    ASSERT_EQ(res, CBM_REFUSAL_CONTEST_UNPROVEN);
    ASSERT_EQ(reg.count, 0);
    PASS();
}

TEST(test_contest_blocking_blocks_promotion) {
    CbmContestRegistry reg;
    cbm_contest_registry_init(&reg);

    CbmContestation c;
    memset(&c, 0, sizeof(c));
    snprintf(c.contest_id, sizeof(c.contest_id), "cnt_2");
    snprintf(c.source_horizon, sizeof(c.source_horizon), "h_adv");
    snprintf(c.target_ref, sizeof(c.target_ref), "cbm://repo/src/api#login");
    c.severity = CBM_CONTEST_BLOCKING;
    snprintf(c.evidence[0], sizeof(c.evidence[0]), "test_failure_repro:CVE-2026-01");
    c.evidence_count = 1;

    char reason[128] = {0};
    CbmRefusalCode res = cbm_contest_submit(&reg, &c, reason, sizeof(reason));
    ASSERT_EQ(res, CBM_REFUSAL_OK);
    ASSERT_EQ(reg.count, 1);

    /* Target is blocked */
    ASSERT_EQ(cbm_contest_is_blocked(&reg, "cbm://repo/src/api#login"), 1);
    ASSERT_EQ(cbm_contest_is_blocked(&reg, "cbm://repo/src/api#other"), 0);

    /* Resolving unblocks */
    ASSERT_TRUE(cbm_contest_resolve(&reg, "cnt_2", "patch_verified_in_session_2"));
    ASSERT_EQ(cbm_contest_is_blocked(&reg, "cbm://repo/src/api#login"), 0);
    PASS();
}

TEST(test_contest_informative_does_not_block) {
    CbmContestRegistry reg;
    cbm_contest_registry_init(&reg);

    CbmContestation c;
    memset(&c, 0, sizeof(c));
    snprintf(c.contest_id, sizeof(c.contest_id), "cnt_info");
    snprintf(c.source_horizon, sizeof(c.source_horizon), "h_critic");
    snprintf(c.target_ref, sizeof(c.target_ref), "cbm://repo/src/db#query");
    c.severity = CBM_CONTEST_INFORMATIVE;
    snprintf(c.evidence[0], sizeof(c.evidence[0]), "perf_benchmark_notice");
    c.evidence_count = 1;

    char reason[128] = {0};
    ASSERT_EQ(cbm_contest_submit(&reg, &c, reason, sizeof(reason)), CBM_REFUSAL_OK);
    /* Informative severity registers question without blocking */
    ASSERT_EQ(cbm_contest_is_blocked(&reg, "cbm://repo/src/db#query"), 0);
    PASS();
}

SUITE(union_contest) {
    RUN_TEST(test_contest_evidence_required);
    RUN_TEST(test_contest_blocking_blocks_promotion);
    RUN_TEST(test_contest_informative_does_not_block);
}
