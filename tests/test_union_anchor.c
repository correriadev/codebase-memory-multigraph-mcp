/*
 * test_union_anchor.c — Scope C02 acceptance criteria (Anchor Kinds).
 */
#include "test_framework.h"
#include "../src/union/union_anchor.h"
#include "../src/union/union_refusal.h"

#include <string.h>

/* Mock file reader */
typedef struct {
    char path[256];
    char content[1024];
} MockFile;

static bool mock_file_reader(const char *path, size_t start, size_t len,
                             char *out_buf, size_t buf_sz, void *ctx) {
    MockFile *file = (MockFile *)ctx;
    if (!file || strcmp(file->path, path) != 0) return false;
    size_t file_len = strlen(file->content);
    if (start >= file_len || start + len > file_len) return false;
    if (len >= buf_sz) return false;

    memcpy(out_buf, file->content + start, len);
    out_buf[len] = '\0';
    return true;
}

/* AC1: LOG_REF whose (session, event_seq) does not exist in log store
 * -> refusal ANCHOR_NOT_FOUND by log lookup. */
TEST(test_anchor_log_ref_not_found) {
    CbmLogStore log_store;
    cbm_log_store_init(&log_store);
    cbm_log_store_append(&log_store, "session_1", "horizon_1", 10, "message at seq 10");

    CbmAnchorEnv env = {0};
    env.log_store = &log_store;

    CbmAnchor anchor = {0};
    anchor.kind = CBM_ANCHOR_LOG_REF;
    strncpy(anchor.log_ref.session_id, "session_1", sizeof(anchor.log_ref.session_id) - 1);
    anchor.log_ref.event_seq = 999; /* does not exist */
    anchor.log_ref.content_hash = 12345;

    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    char reason[128] = {0};
    CbmAnchorVerifyStatus status = cbm_anchor_verify(&anchor, &env, &refusal, reason, sizeof(reason));

    ASSERT_EQ(status, CBM_ANCHOR_VERIFY_NOT_FOUND);
    ASSERT_EQ(refusal, CBM_REFUSAL_ANCHOR_NOT_FOUND);
    ASSERT_TRUE(strstr(reason, "session_1") != NULL);
    PASS();
}

/* AC2: Valid LOG_REF verifies identically forever across later events (replay test). */
TEST(test_anchor_log_ref_replay_immutable) {
    CbmLogStore log_store;
    cbm_log_store_init(&log_store);

    const char *text = "Decision: Use Argon2id for password hashing.";
    uint64_t hash = cbm_anchor_hash_text(text);
    cbm_log_store_append(&log_store, "session_replay", "horizon_1", 42, text);

    CbmAnchorEnv env = {0};
    env.log_store = &log_store;

    CbmAnchor anchor = {0};
    anchor.kind = CBM_ANCHOR_LOG_REF;
    strncpy(anchor.log_ref.session_id, "session_replay", sizeof(anchor.log_ref.session_id) - 1);
    anchor.log_ref.event_seq = 42;
    anchor.log_ref.content_hash = hash;

    /* Verify at time of admission */
    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    CbmAnchorVerifyStatus status = cbm_anchor_verify(&anchor, &env, &refusal, NULL, 0);
    ASSERT_EQ(status, CBM_ANCHOR_VERIFY_OK);
    ASSERT_EQ(refusal, CBM_REFUSAL_OK);

    /* Append many subsequent events to simulate later session activity */
    for (uint64_t seq = 43; seq <= 150; seq++) {
        cbm_log_store_append(&log_store, "session_replay", "horizon_1", seq, "subsequent chatter");
    }

    /* Re-verify at later time: identical match, zero drift */
    refusal = CBM_REFUSAL_OK;
    status = cbm_anchor_verify(&anchor, &env, &refusal, NULL, 0);
    ASSERT_EQ(status, CBM_ANCHOR_VERIFY_OK);
    ASSERT_EQ(refusal, CBM_REFUSAL_OK);
    PASS();
}

/* AC3: FILE_BYTES anchor over drifted file -> verification classifies mismatch, never silent pass. */
TEST(test_anchor_file_bytes_drift_detected) {
    MockFile mock = {0};
    strncpy(mock.path, "docs/architecture.md", sizeof(mock.path) - 1);
    strncpy(mock.content, "Original section text here for testing.", sizeof(mock.content) - 1);

    CbmAnchorEnv env = {0};
    env.file_reader = mock_file_reader;
    env.file_reader_ctx = &mock;

    CbmAnchor anchor = {0};
    anchor.kind = CBM_ANCHOR_FILE_BYTES;
    strncpy(anchor.file_bytes.file_path, "docs/architecture.md", sizeof(anchor.file_bytes.file_path) - 1);
    anchor.file_bytes.byte_start = 0;
    anchor.file_bytes.byte_len = 16;
    strncpy(anchor.file_bytes.expected_text, "Original section", sizeof(anchor.file_bytes.expected_text) - 1);

    /* Initially matches */
    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    CbmAnchorVerifyStatus status = cbm_anchor_verify(&anchor, &env, &refusal, NULL, 0);
    ASSERT_EQ(status, CBM_ANCHOR_VERIFY_OK);

    /* Modify file content so bytes drift */
    strncpy(mock.content, "Modified section text that drifted.", sizeof(mock.content) - 1);

    char reason[128] = {0};
    status = cbm_anchor_verify(&anchor, &env, &refusal, reason, sizeof(reason));
    ASSERT_EQ(status, CBM_ANCHOR_VERIFY_DRIFTED);
    ASSERT_EQ(refusal, CBM_REFUSAL_ANCHOR_NOT_FOUND);
    ASSERT_TRUE(strstr(reason, "byte mismatch") != NULL);
    PASS();
}

/* AC4: DERIVATION_REF whose generator query now yields different output
 * -> flagged invalid-by-derivation with delta referenced. */
TEST(test_anchor_derivation_ref_invalidation) {
    CbmDerivationStore deriv_store;
    cbm_derivation_store_init(&deriv_store);

    const char *query_ref = "cypher:MATCH (m:Module) RETURN count(m)";
    uint64_t initial_hash = 0x1111222233334444ULL;
    cbm_derivation_store_set(&deriv_store, query_ref, initial_hash);

    CbmAnchorEnv env = {0};
    env.derivation_store = &deriv_store;

    CbmAnchor anchor = {0};
    anchor.kind = CBM_ANCHOR_DERIVATION_REF;
    strncpy(anchor.derivation_ref.generator_query_ref, query_ref,
            sizeof(anchor.derivation_ref.generator_query_ref) - 1);
    anchor.derivation_ref.output_hash = initial_hash;

    /* Initially valid */
    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    CbmAnchorVerifyStatus status = cbm_anchor_verify(&anchor, &env, &refusal, NULL, 0);
    ASSERT_EQ(status, CBM_ANCHOR_VERIFY_OK);

    /* Generator output changes (code graph mutated) */
    uint64_t updated_hash = 0x9999888877776666ULL;
    cbm_derivation_store_set(&deriv_store, query_ref, updated_hash);

    char reason[128] = {0};
    status = cbm_anchor_verify(&anchor, &env, &refusal, reason, sizeof(reason));
    ASSERT_EQ(status, CBM_ANCHOR_VERIFY_INVALID_DERIVATION);
    ASSERT_EQ(refusal, CBM_REFUSAL_ANCHOR_NOT_FOUND);
    ASSERT_TRUE(strstr(reason, "derivation output changed") != NULL);
    PASS();
}

/* AC5: Promotion proposal whose anchor kind is absent or malformed
 * -> refusal CLAIM_INVALID naming anchor. */
TEST(test_anchor_absent_or_malformed_refused) {
    CbmAnchor anchor = {0};
    char reason[128] = {0};

    /* Kind is NONE */
    anchor.kind = CBM_ANCHOR_NONE;
    CbmRefusalCode code = cbm_anchor_validate_structure(&anchor, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "anchor") != NULL);

    /* Malformed FILE_BYTES (empty path) */
    memset(&anchor, 0, sizeof(anchor));
    anchor.kind = CBM_ANCHOR_FILE_BYTES;
    code = cbm_anchor_validate_structure(&anchor, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "anchor") != NULL);

    /* Malformed LOG_REF (empty session) */
    memset(&anchor, 0, sizeof(anchor));
    anchor.kind = CBM_ANCHOR_LOG_REF;
    code = cbm_anchor_validate_structure(&anchor, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "anchor") != NULL);

    /* Malformed DERIVATION_REF (empty query) */
    memset(&anchor, 0, sizeof(anchor));
    anchor.kind = CBM_ANCHOR_DERIVATION_REF;
    code = cbm_anchor_validate_structure(&anchor, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "anchor") != NULL);

    PASS();
}

/* AC6: Ground verification reads only from store, zero narrator input. */
TEST(test_anchor_reads_only_from_store) {
    CbmLogStore log_store;
    cbm_log_store_init(&log_store);
    cbm_log_store_append(&log_store, "session_test", "horizon_1", 1, "Verified log entry");

    CbmAnchorEnv env = {0};
    env.log_store = &log_store;

    CbmAnchor anchor = {0};
    anchor.kind = CBM_ANCHOR_LOG_REF;
    strncpy(anchor.log_ref.session_id, "session_test", sizeof(anchor.log_ref.session_id) - 1);
    anchor.log_ref.event_seq = 1;
    anchor.log_ref.content_hash = cbm_anchor_hash_text("Verified log entry");

    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    CbmAnchorVerifyStatus status = cbm_anchor_verify(&anchor, &env, &refusal, NULL, 0);
    ASSERT_EQ(status, CBM_ANCHOR_VERIFY_OK);

    /* Hash mismatch directly in store results in failure without querying anyone */
    anchor.log_ref.content_hash = 0xbadcafeULL;
    status = cbm_anchor_verify(&anchor, &env, &refusal, NULL, 0);
    ASSERT_EQ(status, CBM_ANCHOR_VERIFY_NOT_FOUND);
    ASSERT_EQ(refusal, CBM_REFUSAL_ANCHOR_NOT_FOUND);

    PASS();
}

SUITE(union_anchor) {
    RUN_TEST(test_anchor_log_ref_not_found);
    RUN_TEST(test_anchor_log_ref_replay_immutable);
    RUN_TEST(test_anchor_file_bytes_drift_detected);
    RUN_TEST(test_anchor_derivation_ref_invalidation);
    RUN_TEST(test_anchor_absent_or_malformed_refused);
    RUN_TEST(test_anchor_reads_only_from_store);
}
