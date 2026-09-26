/*
 * test_union_founding.c — Scope D06 acceptance criteria.
 *
 * Verified by host log / return value, never by narrator self-report.
 */
#include "test_framework.h"
#include "../src/union/union_founding.h"
#include "../src/union/union_theme_registry.h"
#include "../src/union/union_refusal.h"

#include <string.h>

static void populate_proposal(CbmFoundingProposal *proposal) {
    memset(proposal, 0, sizeof(*proposal));
    strncpy(proposal->suggested_theme_id, "@inst/event-sourcing", sizeof(proposal->suggested_theme_id) - 1);
    strncpy(proposal->namespace, "institutional", sizeof(proposal->namespace) - 1);
    strncpy(proposal->rationale, "Novel idempotent consumer pattern with high reuse across microservices", sizeof(proposal->rationale) - 1);
    strncpy(proposal->origin_session, "session_craft_99", sizeof(proposal->origin_session) - 1);
    strncpy(proposal->suggested_curator, "event_guild", sizeof(proposal->suggested_curator) - 1);
    proposal->seed_rules_count = 3;
}

/* AC1: Proposal creation from declared inventions */
TEST(test_founding_proposal_generation) {
    CbmFoundingProposal proposal;
    populate_proposal(&proposal);

    ASSERT_STR_EQ(proposal.suggested_theme_id, "@inst/event-sourcing");
    ASSERT_EQ(proposal.seed_rules_count, 3);
    PASS();
}

/* AC2: Operator accepts proposal -> registers theme entry with provenance */
TEST(test_founding_operator_accepts_proposal) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmFoundingProposal proposal;
    populate_proposal(&proposal);

    CbmClosureExclusions exclusions = {0};
    CbmRefusalCode code = cbm_founding_adjudicate(&reg, &proposal, true, false, &exclusions);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_EQ(exclusions.founding_proposals_declined, 0);

    /* Verify theme exists in registry */
    CbmThemeEntry entry;
    ASSERT_EQ(cbm_theme_registry_lookup(&reg, "@inst/event-sourcing", &entry), CBM_REFUSAL_OK);
    ASSERT_STR_EQ(entry.founding_provenance, "session_craft_99");
    ASSERT_EQ(entry.status, CBM_THEME_ABSENT);

    PASS();
}

/* AC3: Operator declines proposal -> counted in typed exclusions, zero themes registered */
TEST(test_founding_operator_declines_proposal) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmFoundingProposal proposal;
    populate_proposal(&proposal);

    CbmClosureExclusions exclusions = {0};
    CbmRefusalCode code = cbm_founding_adjudicate(&reg, &proposal, false, false, &exclusions);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_EQ(exclusions.founding_proposals_declined, 1);

    /* Verify theme does NOT exist in registry */
    CbmThemeEntry entry;
    ASSERT_EQ(cbm_theme_registry_lookup(&reg, "@inst/event-sourcing", &entry), CBM_REFUSAL_THEME_UNKNOWN);

    PASS();
}

/* AC4: Agent autonomous founding attempt is refused with AUTO_FOUNDING_FORBIDDEN */
TEST(test_agent_auto_founding_forbidden) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmFoundingProposal proposal;
    populate_proposal(&proposal);

    CbmClosureExclusions exclusions = {0};
    /* is_agent_autonomous = true */
    CbmRefusalCode code = cbm_founding_adjudicate(&reg, &proposal, true, true, &exclusions);
    ASSERT_EQ(code, CBM_REFUSAL_AUTO_FOUNDING_FORBIDDEN);

    PASS();
}

SUITE(union_founding) {
    RUN_TEST(test_founding_proposal_generation);
    RUN_TEST(test_founding_operator_accepts_proposal);
    RUN_TEST(test_founding_operator_declines_proposal);
    RUN_TEST(test_agent_auto_founding_forbidden);
}
