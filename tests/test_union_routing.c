/*
 * test_union_routing.c — Scope D03 acceptance criteria.
 *
 * Verified by host log / return value, never by narrator self-report.
 */
#include "test_framework.h"
#include "../src/union/union_routing.h"
#include "../src/union/union_theme_registry.h"
#include "../src/union/union_refusal.h"

#include <string.h>

/* AC1: Consultative request classification requires zero theme citations */
TEST(test_routing_consultative_query) {
    CbmActivityClass act = cbm_classify_activity("explain the authentication flow in auth_service.c");
    ASSERT_EQ(act, CBM_ACTIVITY_CONSULTATIVE);

    act = cbm_classify_activity("what does function process_order do?");
    ASSERT_EQ(act, CBM_ACTIVITY_CONSULTATIVE);

    act = cbm_classify_activity("show open questions on the idealization plane");
    ASSERT_EQ(act, CBM_ACTIVITY_CONSULTATIVE);

    PASS();
}

/* AC2: Specialty task classification */
TEST(test_routing_specialty_task) {
    CbmActivityClass act = cbm_classify_activity("implement repository pattern for orders");
    ASSERT_EQ(act, CBM_ACTIVITY_SPECIALTY);

    act = cbm_classify_activity("refactor controller to use dependency injection");
    ASSERT_EQ(act, CBM_ACTIVITY_SPECIALTY);

    act = cbm_classify_activity("create a login screen with clean architecture");
    ASSERT_EQ(act, CBM_ACTIVITY_SPECIALTY);

    PASS();
}

/* AC3: Undeclared invention is refused with PROVENANCE_UNDECLARED */
TEST(test_specialty_undeclared_invention_refused) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmSpecialtyJudgment judgment;
    memset(&judgment, 0, sizeof(judgment));
    judgment.kind = CBM_PROVENANCE_NONE;

    char err[128];
    CbmRefusalCode code = cbm_validate_specialty_provenance(&judgment, &reg, err, sizeof(err));
    ASSERT_EQ(code, CBM_REFUSAL_PROVENANCE_UNDECLARED);
    ASSERT_TRUE(strstr(err, "silent invention") != NULL || strstr(err, "undeclared") != NULL);

    PASS();
}

/* AC4: Valid canon citation passes */
TEST(test_specialty_canon_citation_accepted) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmThemeEntry entry;
    memset(&entry, 0, sizeof(entry));
    strncpy(entry.theme_id, "@inst/clean-arch", sizeof(entry.theme_id) - 1);
    strncpy(entry.namespace, "institutional", sizeof(entry.namespace) - 1);
    strncpy(entry.curator, "lead_arch", sizeof(entry.curator) - 1);
    strncpy(entry.version, "1.0.0", sizeof(entry.version) - 1);
    entry.status = CBM_THEME_ACTIVE;
    ASSERT_EQ(cbm_theme_registry_register(&reg, &entry, NULL, 0), CBM_REFUSAL_OK);

    CbmSpecialtyJudgment judgment;
    memset(&judgment, 0, sizeof(judgment));
    judgment.kind = CBM_PROVENANCE_CANON_CITATION;
    strncpy(judgment.citation.theme_id, "@inst/clean-arch", sizeof(judgment.citation.theme_id) - 1);
    strncpy(judgment.citation.node_uri, "theme://inst/clean-arch/rules/repository", sizeof(judgment.citation.node_uri) - 1);
    strncpy(judgment.citation.pinned_version, "1.0.0", sizeof(judgment.citation.pinned_version) - 1);

    CbmRefusalCode code = cbm_validate_specialty_provenance(&judgment, &reg, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    PASS();
}

/* AC5: Canon citation to unknown theme emits ANCHOR_NOT_FOUND */
TEST(test_specialty_citation_to_unknown_theme_refused) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmSpecialtyJudgment judgment;
    memset(&judgment, 0, sizeof(judgment));
    judgment.kind = CBM_PROVENANCE_CANON_CITATION;
    strncpy(judgment.citation.theme_id, "@inst/non-existent", sizeof(judgment.citation.theme_id) - 1);
    strncpy(judgment.citation.node_uri, "theme://inst/rules/fake", sizeof(judgment.citation.node_uri) - 1);

    CbmRefusalCode code = cbm_validate_specialty_provenance(&judgment, &reg, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_ANCHOR_NOT_FOUND);

    PASS();
}

/* AC6: Declared invention with non-empty rationale is admitted */
TEST(test_specialty_declared_invention_accepted) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmSpecialtyJudgment judgment;
    memset(&judgment, 0, sizeof(judgment));
    judgment.kind = CBM_PROVENANCE_DECLARED_INVENTION;
    judgment.invention.declared = true;
    strncpy(judgment.invention.rationale, "No governing theme for real-time WebSocket protocol; improvising design pattern",
            sizeof(judgment.invention.rationale) - 1);

    CbmRefusalCode code = cbm_validate_specialty_provenance(&judgment, &reg, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    /* Empty rationale must be refused */
    judgment.invention.rationale[0] = '\0';
    code = cbm_validate_specialty_provenance(&judgment, &reg, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_PROVENANCE_UNDECLARED);

    PASS();
}

SUITE(union_routing) {
    RUN_TEST(test_routing_consultative_query);
    RUN_TEST(test_routing_specialty_task);
    RUN_TEST(test_specialty_undeclared_invention_refused);
    RUN_TEST(test_specialty_canon_citation_accepted);
    RUN_TEST(test_specialty_citation_to_unknown_theme_refused);
    RUN_TEST(test_specialty_declared_invention_accepted);
}
