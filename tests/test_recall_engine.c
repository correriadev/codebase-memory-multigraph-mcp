#include "test_framework.h"
#include "../src/admission/recall_engine.h"

TEST(test_recall_engine_init) {
    CbmUri root;
    cbm_uri_parse("cbm://repo/pkg/auth.go#Token", &root);

    RecallReport report;
    int rc = cbm_trigger_recall(NULL, &root, "Anchor drift detected", &report);
    ASSERT_EQ(rc, -1);
    ASSERT_FALSE(report.is_committed);
    PASS();
}

TEST(test_recall_engine_capacity) {
    RecallReport report;
    memset(&report, 0, sizeof(report));
    ASSERT_EQ(sizeof(report.affected_uris) / sizeof(report.affected_uris[0]), CBM_RECALL_MAX_AFFECTED);
    ASSERT_EQ(report.affected_count, 0);
    ASSERT_EQ(report.total_affected_count, 0);
    ASSERT_FALSE(report.is_committed);
    PASS();
}

TEST(test_visited_set_fnv64_collision_exact_uri_distinction) {
    VisitedSet visited = cbm_visited_init();
    const char *uriA = "cbm://repo/src/a.c#SymbolA";
    const char *uriB = "cbm://repo/src/b.c#SymbolB";

    uint64_t synthetic_collision_hash = 0x123456789ABCDEFULL;

    /* Add uriA under synthetic hash */
    ASSERT_TRUE(cbm_visited_add_uri(&visited, synthetic_collision_hash, uriA));

    /* uriA is contained */
    ASSERT_TRUE(cbm_visited_contains_uri(&visited, synthetic_collision_hash, uriA));

    /* uriB sharing identical hash MUST NOT be falsely reported as visited */
    ASSERT_FALSE(cbm_visited_contains_uri(&visited, synthetic_collision_hash, uriB));

    /* Add uriB under the colliding hash */
    ASSERT_TRUE(cbm_visited_add_uri(&visited, synthetic_collision_hash, uriB));

    /* Both distinct URIs are now recognized as visited */
    ASSERT_TRUE(cbm_visited_contains_uri(&visited, synthetic_collision_hash, uriA));
    ASSERT_TRUE(cbm_visited_contains_uri(&visited, synthetic_collision_hash, uriB));

    /* Distinct URI with same hash but never added is still not contained */
    ASSERT_FALSE(cbm_visited_contains_uri(&visited, synthetic_collision_hash, "cbm://repo/src/c.c#SymbolC"));

    cbm_visited_free(&visited);
    PASS();
}

SUITE(recall_engine) {
    RUN_TEST(test_recall_engine_init);
    RUN_TEST(test_recall_engine_capacity);
    RUN_TEST(test_visited_set_fnv64_collision_exact_uri_distinction);
}

