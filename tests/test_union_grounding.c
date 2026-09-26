/*
 * test_union_grounding.c — Scope A03 acceptance criteria.
 */
#include "test_framework.h"
#include "../src/union/union_grounding.h"

#include <string.h>

TEST(test_grounding_provenance_and_epistemic_status) {
    CbmGroundingResultSet rs;
    cbm_grounding_init(&rs);

    ASSERT_TRUE(cbm_grounding_add_item(&rs, "cbm://repo/src/user.c#login", "ADMITTED",
                                      "prov_commit_1", "seq_10", NULL, NULL));
    ASSERT_TRUE(cbm_grounding_add_item(&rs, "cbm://repo/src/user.c#logout", "PROPOSED",
                                      NULL, "seq_10", NULL, NULL));
    ASSERT_TRUE(cbm_grounding_add_item(&rs, "cbm://repo/src/user.c#auth", "CONTESTED",
                                      "prov_commit_2", "seq_10", "cnt_cve", "blocking"));

    ASSERT_EQ(rs.count, 3);

    /* Item 1: Admitted with provenance */
    ASSERT_STR_EQ(rs.items[0].epistemic_status, "ADMITTED");
    ASSERT_FALSE(rs.items[0].provenance_missing);
    ASSERT_FALSE(rs.items[0].is_contested);

    /* Item 2: Proposed without provenance -> flagged PROVENANCE_MISSING (AC4) */
    ASSERT_STR_EQ(rs.items[1].epistemic_status, "PROPOSED");
    ASSERT_TRUE(rs.items[1].provenance_missing);

    /* Item 3: Contested with contest ref & severity (AC3) */
    ASSERT_TRUE(rs.items[2].is_contested);
    ASSERT_STR_EQ(rs.items[2].contest_ref, "cnt_cve");
    ASSERT_STR_EQ(rs.items[2].contest_severity, "blocking");
    PASS();
}

TEST(test_grounding_speculative_isolation) {
    CbmGroundingResultSet raw;
    cbm_grounding_init(&raw);

    cbm_grounding_add_item(&raw, "cbm://repo/src/stable.c", "ADMITTED", "prov_1", "seq_1", NULL, NULL);
    cbm_grounding_add_item(&raw, "cbm://repo/src/speculative.c", "PROPOSED", "prov_2", "seq_1", NULL, NULL);

    /* AC2: Default grounding query (include_speculative=false) isolates speculative content */
    CbmGroundingResultSet filtered;
    cbm_grounding_filter(&raw, false, &filtered);
    ASSERT_EQ(filtered.count, 1);
    ASSERT_STR_EQ(filtered.items[0].uri, "cbm://repo/src/stable.c");

    /* When requested with horizon scope (include_speculative=true) -> returned marked PROPOSED */
    cbm_grounding_filter(&raw, true, &filtered);
    ASSERT_EQ(filtered.count, 2);
    ASSERT_STR_EQ(filtered.items[1].epistemic_status, "PROPOSED");
    PASS();
}

SUITE(union_grounding) {
    RUN_TEST(test_grounding_provenance_and_epistemic_status);
    RUN_TEST(test_grounding_speculative_isolation);
}
