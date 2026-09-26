/*
 * test_union_drift.c — Scope C06 acceptance criteria (Prose Drift Ladder).
 */
#include "test_framework.h"
#include "../src/union/union_drift.h"
#include "../src/union/union_refusal.h"

#include <string.h>

static void populate_file_claim(CbmClaim *claim, const char *id, const char *path) {
    memset(claim, 0, sizeof(*claim));
    strncpy(claim->id, id, sizeof(claim->id) - 1);
    strncpy(claim->predicate, "Claims must have an anchor.", sizeof(claim->predicate) - 1);
    claim->type = CBM_CLAIM_DECISION;
    claim->status = EPISTEMIC_ACCEPTED;

    claim->anchor.kind = CBM_ANCHOR_FILE_BYTES;
    strncpy(claim->anchor.file_bytes.file_path, path, sizeof(claim->anchor.file_bytes.file_path) - 1);
    claim->anchor.file_bytes.byte_start = 10;
    claim->anchor.file_bytes.byte_len = 20;
    strncpy(claim->anchor.file_bytes.expected_text, "Claims must have an",
            sizeof(claim->anchor.file_bytes.expected_text) - 1);
}

/* AC1: Heading renamed with section content intact -> classification lexical,
 * scar recorded, no demotion, URI resolves through rename chain. */
TEST(test_drift_lexical_rename_resolves_chain) {
    CbmClaim claim;
    populate_file_claim(&claim, "cbm://docs/api.md#v1-auth", "docs/api.md");

    CbmDriftScarChain chain;
    cbm_drift_chain_init(&chain);

    CbmRefusalCode code = cbm_drift_process_claim(&claim, CBM_DRIFT_LEXICAL,
                                                 "cbm://docs/api.md#v2-authentication",
                                                 50, "Renamed heading for clarity",
                                                 &chain, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    /* No demotion: status stays ACCEPTED, not suspended, not ungrounded */
    ASSERT_EQ(claim.status, EPISTEMIC_ACCEPTED);
    ASSERT_FALSE(claim.is_suspended);
    ASSERT_FALSE(claim.is_ungrounded);
    ASSERT_STR_EQ(claim.id, "cbm://docs/api.md#v2-authentication");

    /* Scar recorded and URI resolves through chain */
    ASSERT_EQ(chain.count, 1);
    const char *resolved = cbm_drift_resolve_uri(&chain, "cbm://docs/api.md#v1-auth");
    ASSERT_STR_EQ(resolved, "cbm://docs/api.md#v2-authentication");

    PASS();
}

/* AC2: Byte edits inside anchored section -> classification structural,
 * node suspended, grounding read surfaces suspended status (quarantine). */
TEST(test_drift_structural_suspends_node) {
    CbmClaim claim;
    populate_file_claim(&claim, "cbm://docs/auth.md#token", "docs/auth.md");

    CbmDriftScarChain chain;
    cbm_drift_chain_init(&chain);

    CbmRefusalCode code = cbm_drift_process_claim(&claim, CBM_DRIFT_STRUCTURAL,
                                                 NULL, 55, "Bytes modified in section",
                                                 &chain, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    /* Node is suspended/quarantined */
    ASSERT_TRUE(claim.is_suspended);
    ASSERT_FALSE(claim.is_ungrounded);

    PASS();
}

/* AC3: Section deleted -> classification gone, demotion recorded,
 * node remains queryable as ungrounded with scar chain intact. */
TEST(test_drift_gone_demotes_to_ungrounded) {
    CbmClaim claim;
    populate_file_claim(&claim, "cbm://docs/deprecated.md#old-sec", "docs/deprecated.md");

    CbmDriftScarChain chain;
    cbm_drift_chain_init(&chain);

    CbmRefusalCode code = cbm_drift_process_claim(&claim, CBM_DRIFT_GONE,
                                                 NULL, 60, "Section deleted from file",
                                                 &chain, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    /* Demoted to ungrounded */
    ASSERT_TRUE(claim.is_ungrounded);

    /* Scar chain intact */
    ASSERT_EQ(chain.count, 1);
    ASSERT_EQ(chain.scars[0].kind, CBM_DRIFT_GONE);
    ASSERT_STR_EQ(chain.scars[0].before_uri, "cbm://docs/deprecated.md#old-sec");

    PASS();
}

/* AC4: LOG_REF-anchored claim passed to ladder -> refusal LOG_REF_IMMUTABLE.
 * Adversarial test: attempt structural classification on a log anchor. */
TEST(test_drift_log_ref_immutable_refused) {
    CbmClaim claim;
    memset(&claim, 0, sizeof(claim));
    strncpy(claim.id, "claim_chat_dec", sizeof(claim.id) - 1);
    claim.type = CBM_CLAIM_DECISION;
    claim.status = EPISTEMIC_ACCEPTED;

    claim.anchor.kind = CBM_ANCHOR_LOG_REF;
    strncpy(claim.anchor.log_ref.session_id, "session_chat", sizeof(claim.anchor.log_ref.session_id) - 1);
    claim.anchor.log_ref.event_seq = 10;

    CbmDriftScarChain chain;
    cbm_drift_chain_init(&chain);

    char reason[128] = {0};
    CbmRefusalCode code = cbm_drift_process_claim(&claim, CBM_DRIFT_STRUCTURAL,
                                                 NULL, 70, "Attempted mutation",
                                                 &chain, reason, sizeof(reason));

    ASSERT_EQ(code, CBM_REFUSAL_LOG_REF_IMMUTABLE);
    ASSERT_TRUE(strstr(reason, "log-anchored") != NULL || strstr(reason, "immutable") != NULL);

    /* No changes applied to claim */
    ASSERT_FALSE(claim.is_suspended);
    ASSERT_FALSE(claim.is_ungrounded);
    ASSERT_EQ(chain.count, 0);

    PASS();
}

/* AC5: Drift events logged with before/after refs and seq -> scar chain reconstructs history. */
TEST(test_drift_replay_scar_chain) {
    CbmDriftScarChain chain;
    cbm_drift_chain_init(&chain);

    cbm_drift_chain_record(&chain, "cbm://docs/sec.md#sec-1", "cbm://docs/sec.md#sec-1-renamed",
                           CBM_DRIFT_LEXICAL, 10, "rename 1");
    cbm_drift_chain_record(&chain, "cbm://docs/sec.md#sec-1-renamed", "cbm://docs/sec.md#sec-final",
                           CBM_DRIFT_LEXICAL, 20, "rename 2");

    ASSERT_EQ(chain.count, 2);
    /* Replay resolution traverses full chain */
    const char *final_uri = cbm_drift_resolve_uri(&chain, "cbm://docs/sec.md#sec-1");
    ASSERT_STR_EQ(final_uri, "cbm://docs/sec.md#sec-final");

    PASS();
}

/* AC6: Renamed-then-deleted section -> full history (original -> renamed -> gone) in order. */
TEST(test_drift_renamed_then_deleted_history) {
    CbmClaim claim;
    populate_file_claim(&claim, "cbm://docs/rfc.md#draft-1", "docs/rfc.md");

    CbmDriftScarChain chain;
    cbm_drift_chain_init(&chain);

    /* Step 1: Lexical rename */
    (void)cbm_drift_process_claim(&claim, CBM_DRIFT_LEXICAL, "cbm://docs/rfc.md#draft-final",
                                  100, "renamed to final", &chain, NULL, 0);

    /* Step 2: Deleted (gone) */
    (void)cbm_drift_process_claim(&claim, CBM_DRIFT_GONE, NULL,
                                  200, "deleted in clean up", &chain, NULL, 0);

    ASSERT_TRUE(claim.is_ungrounded);

    /* Query history */
    CbmDriftScar history[4];
    size_t count = cbm_drift_query_history(&chain, "cbm://docs/rfc.md#draft-1", history, 4);
    ASSERT_EQ(count, 2);
    ASSERT_EQ(history[0].kind, CBM_DRIFT_LEXICAL);
    ASSERT_STR_EQ(history[0].before_uri, "cbm://docs/rfc.md#draft-1");
    ASSERT_STR_EQ(history[0].after_uri, "cbm://docs/rfc.md#draft-final");

    ASSERT_EQ(history[1].kind, CBM_DRIFT_GONE);
    ASSERT_STR_EQ(history[1].before_uri, "cbm://docs/rfc.md#draft-final");

    PASS();
}

SUITE(union_drift) {
    RUN_TEST(test_drift_lexical_rename_resolves_chain);
    RUN_TEST(test_drift_structural_suspends_node);
    RUN_TEST(test_drift_gone_demotes_to_ungrounded);
    RUN_TEST(test_drift_log_ref_immutable_refused);
    RUN_TEST(test_drift_replay_scar_chain);
    RUN_TEST(test_drift_renamed_then_deleted_history);
}
