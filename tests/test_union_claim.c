/*
 * test_union_claim.c — Scope C01 acceptance criteria (Claim Node Anatomy).
 */
#include "test_framework.h"
#include "../src/union/union_claim.h"
#include "../src/union/union_refusal.h"

#include <string.h>

static void populate_valid_claim(CbmClaim *claim) {
    memset(claim, 0, sizeof(*claim));
    strncpy(claim->id, "claim_auth_jwt_v1", sizeof(claim->id) - 1);
    strncpy(claim->predicate, "Authentication tokens must use RSA signatures with 2048-bit keys.",
            sizeof(claim->predicate) - 1);
    claim->type = CBM_CLAIM_DECISION;
    claim->status = EPISTEMIC_PROPOSED;

    claim->anchor.kind = CBM_ANCHOR_LOG_REF;
    strncpy(claim->anchor.log_ref.session_id, "session_42", sizeof(claim->anchor.log_ref.session_id) - 1);
    strncpy(claim->anchor.log_ref.horizon_id, "horizon_42", sizeof(claim->anchor.log_ref.horizon_id) - 1);
    claim->anchor.log_ref.event_seq = 101;
    claim->anchor.log_ref.content_hash = 0xdeadbeefULL;

    strncpy(claim->provenance.origin_session, "session_42", sizeof(claim->provenance.origin_session) - 1);
    strncpy(claim->provenance.origin_horizon, "horizon_42", sizeof(claim->provenance.origin_horizon) - 1);
    strncpy(claim->provenance.proposed_by, "agent_architect", sizeof(claim->provenance.proposed_by) - 1);
    /* validated_by is empty for newly proposed decision */

    strncpy(claim->consequence, "Compromise of auth token allows forged sessions.",
            sizeof(claim->consequence) - 1);
    strncpy(claim->based_on_seq, "seq_100", sizeof(claim->based_on_seq) - 1);
}

/* AC1: Given a claim missing any mandatory field for its type,
 * When submitted to the horizon,
 * Then refusal CLAIM_INVALID names the exact field. */
TEST(test_claim_missing_mandatory_fields_refused) {
    CbmClaim claim;
    char reason[128];

    /* Missing id */
    populate_valid_claim(&claim);
    claim.id[0] = '\0';
    reason[0] = '\0';
    CbmRefusalCode code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "id") != NULL);

    /* Missing predicate */
    populate_valid_claim(&claim);
    claim.predicate[0] = '\0';
    reason[0] = '\0';
    code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "predicate") != NULL);

    /* Missing origin_session */
    populate_valid_claim(&claim);
    claim.provenance.origin_session[0] = '\0';
    reason[0] = '\0';
    code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "origin_session") != NULL);

    /* Missing anchor */
    populate_valid_claim(&claim);
    claim.anchor.kind = CBM_ANCHOR_NONE;
    reason[0] = '\0';
    code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "anchor") != NULL);

    /* Missing based_on_seq */
    populate_valid_claim(&claim);
    claim.based_on_seq[0] = '\0';
    reason[0] = '\0';
    code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "based_on_seq") != NULL);

    /* VERDICT requires evidence refs */
    populate_valid_claim(&claim);
    claim.type = CBM_CLAIM_VERDICT;
    claim.evidence_count = 0;
    reason[0] = '\0';
    code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "evidence") != NULL);

    PASS();
}

/* AC2: Given a predicate containing a blacklisted deictic phrase,
 * When validated,
 * Then refusal PREDICATE_NOT_SELF_CONTAINED with the phrase cited in the reason. */
TEST(test_claim_deictic_blacklist_refused) {
    cbm_claim_blacklist_reset();
    CbmClaim claim;
    char reason[128];

    const char *bad_phrases[] = {
        "as discussed above",
        "the above",
        "aforementioned",
        "as previously",
        "como discutido acima"
    };

    for (size_t i = 0; i < sizeof(bad_phrases) / sizeof(bad_phrases[0]); i++) {
        populate_valid_claim(&claim);
        snprintf(claim.predicate, sizeof(claim.predicate),
                 "The system should follow %s requirements.", bad_phrases[i]);
        reason[0] = '\0';
        CbmRefusalCode code = cbm_claim_validate(&claim, reason, sizeof(reason));
        ASSERT_EQ(code, CBM_REFUSAL_PREDICATE_NOT_SELF_CONTAINED);
        ASSERT_TRUE(strstr(reason, bad_phrases[i]) != NULL);
    }

    PASS();
}

/* AC3: Given a well-formed, self-contained claim,
 * When submitted,
 * Then it exists in the session horizon with status=PROPOSED and all anatomy fields —
 * and does not exist in any base graph. */
TEST(test_claim_well_formed_submitted_to_horizon) {
    CbmClaimStore store;
    cbm_claim_store_init(&store, "horizon_sess_1");

    CbmClaim claim;
    populate_valid_claim(&claim);
    claim.status = EPISTEMIC_SHADOWED; /* ensure submission forces PROPOSED */

    char reason[128];
    CbmRefusalCode code = cbm_claim_submit_to_horizon(&store, &claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    const CbmClaim *retrieved = cbm_claim_store_get(&store, claim.id);
    ASSERT_NOT_NULL(retrieved);
    ASSERT_STR_EQ(retrieved->id, claim.id);
    ASSERT_STR_EQ(retrieved->predicate, claim.predicate);
    ASSERT_EQ(retrieved->type, CBM_CLAIM_DECISION);
    ASSERT_EQ(retrieved->status, EPISTEMIC_PROPOSED);
    ASSERT_STR_EQ(retrieved->provenance.origin_session, "session_42");
    ASSERT_STR_EQ(retrieved->provenance.proposed_by, "agent_architect");
    ASSERT_EQ(retrieved->anchor.kind, CBM_ANCHOR_LOG_REF);

    /* Invariance: claim does NOT exist in an unrelated store / base graph */
    CbmClaimStore base_graph_store;
    cbm_claim_store_init(&base_graph_store, "base_graph_prod");
    ASSERT_NULL(cbm_claim_store_get(&base_graph_store, claim.id));

    PASS();
}

/* AC4: Given type=OPEN_QUESTION without consequence,
 * When validated,
 * Then refusal CLAIM_INVALID naming consequence. */
TEST(test_claim_open_question_requires_consequence) {
    CbmClaim claim;
    populate_valid_claim(&claim);
    claim.type = CBM_CLAIM_OPEN_QUESTION;
    claim.consequence[0] = '\0'; /* missing consequence */

    char reason[128];
    CbmRefusalCode code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "consequence") != NULL);

    /* With consequence, passes */
    strncpy(claim.consequence, "Data loss if replica becomes active with stale state.",
            sizeof(claim.consequence) - 1);
    code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    PASS();
}

/* AC5: Self-containment replay test: retrieve the claim without its origin session data
 * and assert every retrieval question of ADR_V1 §3.7 (what/consequence/validator/seq/anchor)
 * is answerable from the node alone. */
TEST(test_claim_self_containment_replay) {
    CbmClaimStore store;
    cbm_claim_store_init(&store, "horizon_replay");

    CbmClaim claim;
    populate_valid_claim(&claim);
    strncpy(claim.provenance.validated_by, "operator_alice", sizeof(claim.provenance.validated_by) - 1);

    CbmRefusalCode code = cbm_claim_submit_to_horizon(&store, &claim, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    /* Retrieve by ID only, with no session context passed */
    const CbmClaim *node = cbm_claim_store_get(&store, "claim_auth_jwt_v1");
    ASSERT_NOT_NULL(node);

    /* What: predicate */
    ASSERT_STR_EQ(node->predicate, "Authentication tokens must use RSA signatures with 2048-bit keys.");
    /* Consequence */
    ASSERT_STR_EQ(node->consequence, "Compromise of auth token allows forged sessions.");
    /* Validator */
    ASSERT_STR_EQ(node->provenance.validated_by, "operator_alice");
    /* Sequence coordinate */
    ASSERT_STR_EQ(node->based_on_seq, "seq_100");
    /* Anchor ground */
    ASSERT_EQ(node->anchor.kind, CBM_ANCHOR_LOG_REF);
    ASSERT_EQ(node->anchor.log_ref.event_seq, 101);

    PASS();
}

/* AC6: Given the blacklist, When extended (new deictic phrase),
 * Then the taxonomy supersedes with version record — old refusals remain resolvable. */
TEST(test_claim_blacklist_extension) {
    cbm_claim_blacklist_reset();

    CbmClaim claim;
    populate_valid_claim(&claim);
    strncpy(claim.predicate, "The protocol follows that which was previously stated.",
            sizeof(claim.predicate) - 1);

    /* Before extension, this phrase is not blacklisted */
    char reason[128];
    CbmRefusalCode code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    /* Extend blacklist with new phrase */
    bool added = cbm_claim_blacklist_add("previously stated");
    ASSERT_TRUE(added);

    /* Now it must be refused */
    code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_PREDICATE_NOT_SELF_CONTAINED);
    ASSERT_TRUE(strstr(reason, "previously stated") != NULL);

    /* Old refusal codes and baseline blacklist remain resolvable */
    populate_valid_claim(&claim);
    strncpy(claim.predicate, "The protocol is as discussed above.", sizeof(claim.predicate) - 1);
    code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_PREDICATE_NOT_SELF_CONTAINED);
    ASSERT_TRUE(strstr(reason, "as discussed above") != NULL);

    cbm_claim_blacklist_reset();
    PASS();
}

/* One-sentence rule: length cap, no newline, single terminator */
TEST(test_claim_one_sentence_rule) {
    cbm_claim_blacklist_reset();
    CbmClaim claim;
    char reason[128];

    /* Newline in predicate */
    populate_valid_claim(&claim);
    strncpy(claim.predicate, "Sentence one.\nSentence two.", sizeof(claim.predicate) - 1);
    CbmRefusalCode code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_PREDICATE_NOT_SELF_CONTAINED);
    ASSERT_TRUE(strstr(reason, "newline") != NULL);

    /* Multiple sentences */
    populate_valid_claim(&claim);
    strncpy(claim.predicate, "First sentence. Second sentence.", sizeof(claim.predicate) - 1);
    code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_PREDICATE_NOT_SELF_CONTAINED);
    ASSERT_TRUE(strstr(reason, "single sentence") != NULL);

    /* Missing terminator at end */
    populate_valid_claim(&claim);
    strncpy(claim.predicate, "Missing terminator sentence", sizeof(claim.predicate) - 1);
    code = cbm_claim_validate(&claim, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_PREDICATE_NOT_SELF_CONTAINED);
    ASSERT_TRUE(strstr(reason, "terminator") != NULL);

    PASS();
}

SUITE(union_claim) {
    RUN_TEST(test_claim_missing_mandatory_fields_refused);
    RUN_TEST(test_claim_deictic_blacklist_refused);
    RUN_TEST(test_claim_well_formed_submitted_to_horizon);
    RUN_TEST(test_claim_open_question_requires_consequence);
    RUN_TEST(test_claim_self_containment_replay);
    RUN_TEST(test_claim_blacklist_extension);
    RUN_TEST(test_claim_one_sentence_rule);
}
