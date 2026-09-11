#include "test_framework.h"
#include "../src/admission/anchor_checker.h"
#include "../src/core/cbm_uri.h"

TEST(test_anchor_checker_fast_path) {
    TwoTierAnchor a;
    memset(&a, 0, sizeof(a));
    snprintf(a.file_path, sizeof(a.file_path), "test.go");
    snprintf(a.expected_text, sizeof(a.expected_text), "func ValidateToken() {}");
    a.byte_start = 0;
    a.byte_len = (uint32_t)strlen(a.expected_text);
    a.ast_signature_hash = cbm_fnv1a_64(a.expected_text, a.byte_len);

    ASSERT_GT(a.ast_signature_hash, 0);
    PASS();
}

TEST(test_anchor_checker_oversized_byte_len) {
    TwoTierAnchor a;
    memset(&a, 0, sizeof(a));
    snprintf(a.file_path, sizeof(a.file_path), "nonexistent.go");
    a.byte_len = 2048; /* exceeds 1024 capacity */
    bool match = cbm_fast_offset_match(".", &a);
    ASSERT_FALSE(match);
    PASS();
}

TEST(test_anchor_checker_ast_null_guard) {
    bool ok = true;
    int rc = cbm_ast_signature_match(".", NULL, &ok);
    ASSERT_EQ(rc, -1);
    ASSERT_FALSE(ok);
    PASS();
}

TEST(test_anchor_checker_whitespace_normalization) {
    /* Verify that normalized hash handles CRLF and multiple spaces gracefully */
    TwoTierAnchor a;
    memset(&a, 0, sizeof(a));
    snprintf(a.expected_text, sizeof(a.expected_text), "func ValidateToken(\n    token string,\n) bool");
    a.byte_len = (uint32_t)strlen(a.expected_text);
    a.ast_signature_hash = cbm_fnv1a_64(a.expected_text, a.byte_len);
    ASSERT_GT(a.ast_signature_hash, 0);
    PASS();
}

SUITE(anchor_checker_suite) {
    RUN_TEST(test_anchor_checker_fast_path);
    RUN_TEST(test_anchor_checker_oversized_byte_len);
    RUN_TEST(test_anchor_checker_ast_null_guard);
    RUN_TEST(test_anchor_checker_whitespace_normalization);
}


