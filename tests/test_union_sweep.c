/*
 * test_union_sweep.c — Scope C03 acceptance criteria (Conversational Birth & Closure Sweep).
 */
#include "test_framework.h"
#include "../src/union/union_sweep.h"
#include "../src/union/union_refusal.h"

#include <string.h>

static void populate_basic_claim(CbmClaim *claim, const char *id, CbmClaimType type, const char *predicate) {
    memset(claim, 0, sizeof(*claim));
    strncpy(claim->id, id, sizeof(claim->id) - 1);
    strncpy(claim->predicate, predicate, sizeof(claim->predicate) - 1);
    claim->type = type;
    claim->status = EPISTEMIC_PROPOSED;
    strncpy(claim->consequence, "Consequence if ignored.", sizeof(claim->consequence) - 1);
    strncpy(claim->based_on_seq, "seq_50", sizeof(claim->based_on_seq) - 1);
}

/* AC1: Operator decision captured mid-session -> PROPOSED claim in horizon
 * with provenance.proposed_by=agent, validated_by=∅, and a LOG_REF anchor. */
TEST(test_sweep_capture_conversational_claim) {
    CbmSessionSweepContext ctx;
    cbm_sweep_init(&ctx, "session_chat_1", "horizon_chat_1");

    CbmClaim claim;
    populate_basic_claim(&claim, "claim_dec_crypto", CBM_CLAIM_DECISION,
                         "Use AES-256-GCM for envelope encryption.");

    char reason[128] = {0};
    CbmRefusalCode code = cbm_sweep_capture_claim(&ctx, &claim, 12, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    CbmClaim *saved = cbm_sweep_find_claim(&ctx, "claim_dec_crypto");
    ASSERT_NOT_NULL(saved);
    ASSERT_STR_EQ(saved->provenance.proposed_by, "agent");
    ASSERT_STR_EQ(saved->provenance.validated_by, ""); /* ∅ */
    ASSERT_EQ(saved->status, EPISTEMIC_PROPOSED);
    ASSERT_EQ(saved->anchor.kind, CBM_ANCHOR_LOG_REF);
    ASSERT_STR_EQ(saved->anchor.log_ref.session_id, "session_chat_1");
    ASSERT_EQ(saved->anchor.log_ref.event_seq, 12);

    PASS();
}

/* AC2: DECISION claim promoted with validated_by=∅ -> refusal naming validated_by. */
TEST(test_sweep_decision_without_operator_validation_refused) {
    CbmSessionSweepContext ctx;
    cbm_sweep_init(&ctx, "session_chat_2", "horizon_chat_2");

    CbmClaim claim;
    populate_basic_claim(&claim, "claim_dec_unvalidated", CBM_CLAIM_DECISION,
                         "Use PostgreSQL for storage.");
    (void)cbm_sweep_capture_claim(&ctx, &claim, 1, NULL, 0);

    /* Attempt to assign destination PROMOTED without operator validation */
    char reason[128] = {0};
    CbmRefusalCode code = cbm_sweep_assign_destination(&ctx, "claim_dec_unvalidated",
                                                      CBM_SWEEP_DEST_PROMOTED, NULL,
                                                      reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "validated_by") != NULL);

    PASS();
}

/* AC3: FACT claim with evidence refs and no operator validation
 * -> promotion proceeds through blind gate on evidence alone. */
TEST(test_sweep_fact_promotes_on_evidence_alone) {
    CbmSessionSweepContext ctx;
    cbm_sweep_init(&ctx, "session_chat_3", "horizon_chat_3");

    CbmClaim claim;
    populate_basic_claim(&claim, "claim_fact_benchmark", CBM_CLAIM_FACT,
                         "Latency is under 15ms at p99.");
    strncpy(claim.evidence_refs[0], "benchmark_run_2026_09", sizeof(claim.evidence_refs[0]) - 1);
    claim.evidence_count = 1;

    (void)cbm_sweep_capture_claim(&ctx, &claim, 5, NULL, 0);

    /* Assign destination PROMOTED without operator validation — facts admit on evidence */
    char reason[128] = {0};
    CbmRefusalCode code = cbm_sweep_assign_destination(&ctx, "claim_fact_benchmark",
                                                      CBM_SWEEP_DEST_PROMOTED, NULL,
                                                      reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    CbmClaim *saved = cbm_sweep_find_claim(&ctx, "claim_fact_benchmark");
    ASSERT_NOT_NULL(saved);
    ASSERT_EQ(saved->status, EPISTEMIC_ACCEPTED);

    PASS();
}

/* AC4: Session close with 2 PROPOSED claims unresolved
 * -> SWEEP_INCOMPLETE logged, closure blocked, 2 claims listed. */
TEST(test_sweep_incomplete_blocks_closure) {
    CbmSessionSweepContext ctx;
    cbm_sweep_init(&ctx, "session_chat_4", "horizon_chat_4");

    CbmClaim c1, c2;
    populate_basic_claim(&c1, "claim_prop_1", CBM_CLAIM_OPEN_QUESTION, "Which cloud provider to pick?");
    populate_basic_claim(&c2, "claim_prop_2", CBM_CLAIM_DECISION, "Use Redis for cache layer.");

    (void)cbm_sweep_capture_claim(&ctx, &c1, 1, NULL, 0);
    (void)cbm_sweep_capture_claim(&ctx, &c2, 2, NULL, 0);

    char unresolved_list[256] = {0};
    char reason[128] = {0};
    CbmRefusalCode code = cbm_sweep_close_session(&ctx, NULL, unresolved_list, sizeof(unresolved_list),
                                                 reason, sizeof(reason));

    ASSERT_EQ(code, CBM_REFUSAL_SWEEP_INCOMPLETE);
    ASSERT_FALSE(ctx.is_closed);
    ASSERT_TRUE(strstr(unresolved_list, "claim_prop_1") != NULL);
    ASSERT_TRUE(strstr(unresolved_list, "claim_prop_2") != NULL);

    PASS();
}

/* AC5: Completed sweep -> closure event written, every claim's destination recorded,
 * discarded claims appear as typed exclusion counts, never content. */
TEST(test_sweep_completed_exclusion_counts_never_content) {
    CbmSessionSweepContext ctx;
    cbm_sweep_init(&ctx, "session_chat_5", "horizon_chat_5");

    CbmClaim c1, c2, c3;
    populate_basic_claim(&c1, "claim_promoted", CBM_CLAIM_DECISION, "Use JSON for API responses.");
    populate_basic_claim(&c2, "claim_converted", CBM_CLAIM_OPEN_QUESTION, "What is the token budget?");
    populate_basic_claim(&c3, "claim_discarded", CBM_CLAIM_DECISION, "Consider using MongoDB.");

    (void)cbm_sweep_capture_claim(&ctx, &c1, 1, NULL, 0);
    (void)cbm_sweep_capture_claim(&ctx, &c2, 2, NULL, 0);
    (void)cbm_sweep_capture_claim(&ctx, &c3, 3, NULL, 0);

    /* 1. Operator validates c1 and promotes */
    cbm_sweep_validate_intent(&ctx, "claim_promoted", "operator", NULL, 0);
    cbm_sweep_assign_destination(&ctx, "claim_promoted", CBM_SWEEP_DEST_PROMOTED, NULL, NULL, 0);

    /* 2. Convert c2 to open question with owner */
    cbm_sweep_assign_destination(&ctx, "claim_converted", CBM_SWEEP_DEST_CONVERTED_OPEN_QUESTION,
                                 "operator_architect", NULL, 0);

    /* 3. Discard c3 as exploration */
    cbm_sweep_assign_destination(&ctx, "claim_discarded", CBM_SWEEP_DEST_DISCARDED,
                                 "exploration", NULL, 0);

    /* Now close session */
    char unresolved[128] = {0};
    CbmRefusalCode code = cbm_sweep_close_session(&ctx, NULL, unresolved, sizeof(unresolved), NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_TRUE(ctx.is_closed);

    /* Check sweep report counts */
    ASSERT_EQ(ctx.report.promoted_count, 1);
    ASSERT_EQ(ctx.report.converted_count, 1);
    ASSERT_EQ(ctx.report.discarded_exploration_count, 1);
    ASSERT_EQ(ctx.report.discarded_total_count, 1);

    /* The discarded claim content is purged/cleared from live store — counts only */
    CbmClaim *discarded = cbm_sweep_find_claim(&ctx, "claim_discarded");
    ASSERT_STR_EQ(discarded->predicate, ""); /* content wiped */

    PASS();
}

/* AC6: Any captured claim -> visible before any validation (transparency event). */
TEST(test_sweep_capture_transparency_visible) {
    CbmSessionSweepContext ctx;
    cbm_sweep_init(&ctx, "session_chat_6", "horizon_chat_6");

    CbmClaim claim;
    populate_basic_claim(&claim, "claim_transp", CBM_CLAIM_DECISION,
                         "Adopt semver for versioning.");

    (void)cbm_sweep_capture_claim(&ctx, &claim, 9, NULL, 0);

    /* Immediately queryable in live session */
    CbmClaim *retrieved = cbm_sweep_find_claim(&ctx, "claim_transp");
    ASSERT_NOT_NULL(retrieved);
    ASSERT_EQ(retrieved->status, EPISTEMIC_PROPOSED);
    ASSERT_STR_EQ(retrieved->provenance.validated_by, "");

    PASS();
}

/* AC7: Adversarial test: agent attempts to fill validated_by with its own identity
 * -> refusal CLAIM_INVALID naming validated_by. */
TEST(test_sweep_agent_self_validation_refused) {
    CbmSessionSweepContext ctx;
    cbm_sweep_init(&ctx, "session_chat_7", "horizon_chat_7");

    CbmClaim claim;
    populate_basic_claim(&claim, "claim_adversarial", CBM_CLAIM_DECISION,
                         "Self-approved architectural change.");
    (void)cbm_sweep_capture_claim(&ctx, &claim, 1, NULL, 0);

    char reason[128] = {0};
    /* Agent tries to validate as "agent" or "assistant" or "self" */
    CbmRefusalCode code = cbm_sweep_validate_intent(&ctx, "claim_adversarial", "agent",
                                                   reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "validated_by") != NULL);

    code = cbm_sweep_validate_intent(&ctx, "claim_adversarial", "assistant_bot",
                                    reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "validated_by") != NULL);

    PASS();
}

SUITE(union_sweep) {
    RUN_TEST(test_sweep_capture_conversational_claim);
    RUN_TEST(test_sweep_decision_without_operator_validation_refused);
    RUN_TEST(test_sweep_fact_promotes_on_evidence_alone);
    RUN_TEST(test_sweep_incomplete_blocks_closure);
    RUN_TEST(test_sweep_completed_exclusion_counts_never_content);
    RUN_TEST(test_sweep_capture_transparency_visible);
    RUN_TEST(test_sweep_agent_self_validation_refused);
}
