/*
 * test_union_cross_territory.c — Scope D04 acceptance criteria.
 *
 * Verified by host log / return value, never by narrator self-report.
 */
#include "test_framework.h"
#include "../src/union/union_cross_territory.h"
#include "../src/union/union_refusal.h"

#include <string.h>

/* AC1: Valid cross-territory typed edges */
TEST(test_cross_territory_edge_types) {
    CbmCrossEdge edge;
    memset(&edge, 0, sizeof(edge));
    strncpy(edge.source_project_node, "symbol://src/user_repo.c/UserRepo", sizeof(edge.source_project_node) - 1);
    strncpy(edge.target_theme_uri, "theme://inst/clean-arch/rules/repository", sizeof(edge.target_theme_uri) - 1);
    edge.type = CBM_CROSS_EDGE_CONFORMS_TO;

    CbmRefusalCode code = cbm_cross_edge_validate(&edge, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    edge.type = CBM_CROSS_EDGE_DEVIATES_FROM;
    code = cbm_cross_edge_validate(&edge, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    edge.type = CBM_CROSS_EDGE_CONSULTS;
    code = cbm_cross_edge_validate(&edge, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    /* Invalid type */
    edge.type = (CbmCrossEdgeType)99;
    char err[128];
    code = cbm_cross_edge_validate(&edge, err, sizeof(err));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(err, "edge type") != NULL);

    PASS();
}

/* AC2: Project session write attempt into theme territory is refused */
TEST(test_non_write_barrier_blocks_project_writes) {
    char err[128];
    CbmRefusalCode code = cbm_cross_territory_check_write(CBM_CONTEXT_PROJECT_SESSION,
                                                          "theme://inst/clean-arch/rules/new_rule",
                                                          err, sizeof(err));
    ASSERT_EQ(code, CBM_REFUSAL_TERRITORY_WRITE_FORBIDDEN);
    ASSERT_TRUE(strstr(err, "read-only") != NULL || strstr(err, "forbidden") != NULL);

    /* Project writing to its own graph is permitted */
    code = cbm_cross_territory_check_write(CBM_CONTEXT_PROJECT_SESSION,
                                           "project://my_repo/src/main.c",
                                           err, sizeof(err));
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    PASS();
}

/* AC3: Curator workflow is permitted to write to theme graphs */
TEST(test_curator_write_allowed) {
    CbmRefusalCode code = cbm_cross_territory_check_write(CBM_CONTEXT_CURATOR_WORKFLOW,
                                                          "theme://inst/clean-arch/rules/new_rule",
                                                          NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    PASS();
}

/* AC4: Contestation is surfaced when project claim conflicts with bound theme */
TEST(test_cross_territory_contestation_surfaced) {
    CbmContestationEvent event;
    memset(&event, 0, sizeof(event));

    bool contest = cbm_detect_cross_territory_conflict(
        "claim://dec_use_mongo",
        "theme://inst/rules/postgres_only",
        &event
    );
    ASSERT_TRUE(contest);
    ASSERT_EQ(event.status, CBM_CONTESTATION_OPEN);
    ASSERT_STR_EQ(event.project_claim_uri, "claim://dec_use_mongo");
    ASSERT_STR_EQ(event.theme_rule_uri, "theme://inst/rules/postgres_only");

    PASS();
}

SUITE(union_cross_territory) {
    RUN_TEST(test_cross_territory_edge_types);
    RUN_TEST(test_non_write_barrier_blocks_project_writes);
    RUN_TEST(test_curator_write_allowed);
    RUN_TEST(test_cross_territory_contestation_surfaced);
}
