/*
 * test_union_refusal.c — Scope A04 acceptance criteria.
 *
 * Verified by host log / return value, never by narrator self-report.
 */
#include "test_framework.h"
#include "../src/union/union_refusal.h"

#include <string.h>

/* AC1: every refusable condition emits the specific code (not free text). */
TEST(test_refusal_code_roundtrip) {
    for (int code = CBM_REFUSAL_CONTRACT_INVALID; code < CBM_REFUSAL_CODE_COUNT; code++) {
        const char *name = cbm_refusal_code_string((CbmRefusalCode)code);
        ASSERT_STR_NEQ(name, "UNKNOWN");
        ASSERT_STR_NEQ(name, "");
        CbmRefusalCode back = cbm_refusal_code_from_string(name);
        ASSERT_EQ(back, code);
    }
    PASS();
}

/* Free-text refusals are a conformance failure: unknown strings map to OK. */
TEST(test_refusal_unknown_string_not_a_code) {
    ASSERT_EQ(cbm_refusal_code_from_string("because_i_said_so"), CBM_REFUSAL_OK);
    ASSERT_EQ(cbm_refusal_code_from_string(""), CBM_REFUSAL_OK);
    ASSERT_EQ(cbm_refusal_code_from_string(NULL), CBM_REFUSAL_OK);
    ASSERT_STR_EQ(cbm_refusal_code_string((CbmRefusalCode)999), "UNKNOWN");
    ASSERT_FALSE(cbm_refusal_is_valid(CBM_REFUSAL_OK));
    ASSERT_FALSE(cbm_refusal_is_valid((CbmRefusalCode)999));
    ASSERT_TRUE(cbm_refusal_is_valid(CBM_REFUSAL_STALE_BASE));
    PASS();
}

/* AC1 (client obligation): each code carries a mandated behavior. */
TEST(test_refusal_every_code_has_obligation) {
    for (int code = CBM_REFUSAL_CONTRACT_INVALID; code < CBM_REFUSAL_CODE_COUNT; code++) {
        const char *obligation = cbm_refusal_client_obligation((CbmRefusalCode)code);
        ASSERT_NOT_NULL(obligation);
        ASSERT_STR_NEQ(obligation, "");
    }
    PASS();
}

/* AC2: identical re-submission is detected and re-refused. */
TEST(test_retry_identical_detected) {
    CbmRefusalLedger ledger;
    cbm_refusal_ledger_init(&ledger);

    CbmRefusalCode effective = CBM_REFUSAL_OK;
    bool retry = cbm_refusal_ledger_check_and_record(&ledger, "horizon_a",
                                                     CBM_REFUSAL_ANCHOR_NOT_FOUND,
                                                     "claim://x", &effective);
    ASSERT_FALSE(retry);
    ASSERT_EQ(effective, CBM_REFUSAL_ANCHOR_NOT_FOUND);

    retry = cbm_refusal_ledger_check_and_record(&ledger, "horizon_a",
                                                CBM_REFUSAL_ANCHOR_NOT_FOUND,
                                                "claim://x", &effective);
    ASSERT_TRUE(retry);
    ASSERT_EQ(effective, CBM_REFUSAL_RETRY_IDENTICAL);
    PASS();
}

/* A different payload records normally. */
TEST(test_retry_different_payload_ok) {
    CbmRefusalLedger ledger;
    cbm_refusal_ledger_init(&ledger);

    CbmRefusalCode effective = CBM_REFUSAL_OK;
    (void)cbm_refusal_ledger_check_and_record(&ledger, "horizon_a",
                                              CBM_REFUSAL_ANCHOR_NOT_FOUND,
                                              "claim://x", &effective);
    bool retry = cbm_refusal_ledger_check_and_record(&ledger, "horizon_a",
                                                     CBM_REFUSAL_ANCHOR_NOT_FOUND,
                                                     "claim://y", &effective);
    ASSERT_FALSE(retry);
    ASSERT_EQ(effective, CBM_REFUSAL_ANCHOR_NOT_FOUND);
    PASS();
}

/* Same payload in a different horizon is a new refusal event. */
TEST(test_retry_cross_horizon_ok) {
    CbmRefusalLedger ledger;
    cbm_refusal_ledger_init(&ledger);

    CbmRefusalCode effective = CBM_REFUSAL_OK;
    (void)cbm_refusal_ledger_check_and_record(&ledger, "horizon_a",
                                              CBM_REFUSAL_ANCHOR_NOT_FOUND,
                                              "claim://x", &effective);
    bool retry = cbm_refusal_ledger_check_and_record(&ledger, "horizon_b",
                                                     CBM_REFUSAL_ANCHOR_NOT_FOUND,
                                                     "claim://x", &effective);
    ASSERT_FALSE(retry);
    PASS();
}

/* Different code over the same payload is a new refusal event. */
TEST(test_retry_different_code_ok) {
    CbmRefusalLedger ledger;
    cbm_refusal_ledger_init(&ledger);

    CbmRefusalCode effective = CBM_REFUSAL_OK;
    (void)cbm_refusal_ledger_check_and_record(&ledger, "horizon_a",
                                              CBM_REFUSAL_ANCHOR_NOT_FOUND,
                                              "claim://x", &effective);
    bool retry = cbm_refusal_ledger_check_and_record(&ledger, "horizon_a",
                                                     CBM_REFUSAL_EVIDENCE_REQUIRED,
                                                     "claim://x", &effective);
    ASSERT_FALSE(retry);
    ASSERT_EQ(effective, CBM_REFUSAL_EVIDENCE_REQUIRED);
    PASS();
}

/* The ledger is bounded: beyond capacity the oldest entry is evicted, no
 * crash, no unbounded growth. */
TEST(test_ledger_capacity_bounded) {
    CbmRefusalLedger ledger;
    cbm_refusal_ledger_init(&ledger);

    char payload[32];
    for (int i = 0; i < CBM_REFUSAL_LEDGER_CAP + 50; i++) {
        snprintf(payload, sizeof(payload), "payload_%d", i);
        (void)cbm_refusal_ledger_check_and_record(&ledger, "horizon_cap",
                                                  CBM_REFUSAL_STALE_BASE, payload, NULL);
    }
    ASSERT_EQ(ledger.count, CBM_REFUSAL_LEDGER_CAP);

    /* The first entries were evicted: payload_0 must NOT be retry-identical. */
    CbmRefusalCode effective = CBM_REFUSAL_OK;
    bool retry = cbm_refusal_ledger_check_and_record(&ledger, "horizon_cap",
                                                     CBM_REFUSAL_STALE_BASE,
                                                     "payload_0", &effective);
    ASSERT_FALSE(retry);

    /* A recent entry (not yet evicted) still detects. */
    snprintf(payload, sizeof(payload), "payload_%d", CBM_REFUSAL_LEDGER_CAP + 40);
    retry = cbm_refusal_ledger_check_and_record(&ledger, "horizon_cap",
                                                CBM_REFUSAL_STALE_BASE, payload, &effective);
    ASSERT_TRUE(retry);
    ASSERT_EQ(effective, CBM_REFUSAL_RETRY_IDENTICAL);
    PASS();
}

/* The fingerprint separates parts: "a"+"bc" != "ab"+"c". */
TEST(test_fingerprint_part_separation) {
    uint64_t a = cbm_refusal_fingerprint("ab", CBM_REFUSAL_STALE_BASE, "c");
    uint64_t b = cbm_refusal_fingerprint("a", CBM_REFUSAL_STALE_BASE, "bc");
    ASSERT_NEQ(a, b);

    /* Deterministic. */
    ASSERT_EQ(a, cbm_refusal_fingerprint("ab", CBM_REFUSAL_STALE_BASE, "c"));
    /* Never the empty-slot marker. */
    for (int code = CBM_REFUSAL_CONTRACT_INVALID; code < CBM_REFUSAL_CODE_COUNT; code++) {
        ASSERT_NEQ(cbm_refusal_fingerprint("", (CbmRefusalCode)code, ""), 0);
    }
    PASS();
}

TEST(test_track_d_refusal_codes_exist) {
    ASSERT_STR_EQ(cbm_refusal_code_string(CBM_REFUSAL_PROVENANCE_UNDECLARED), "PROVENANCE_UNDECLARED");
    ASSERT_STR_EQ(cbm_refusal_code_string(CBM_REFUSAL_BINDING_SELF_VALIDATED), "BINDING_SELF_VALIDATED");
    ASSERT_STR_EQ(cbm_refusal_code_string(CBM_REFUSAL_TERRITORY_WRITE_FORBIDDEN), "TERRITORY_WRITE_FORBIDDEN");
    ASSERT_STR_EQ(cbm_refusal_code_string(CBM_REFUSAL_AUTO_FOUNDING_FORBIDDEN), "AUTO_FOUNDING_FORBIDDEN");
    ASSERT_STR_EQ(cbm_refusal_code_string(CBM_REFUSAL_THEME_SCHEMA_INVALID), "THEME_SCHEMA_INVALID");
    ASSERT_STR_EQ(cbm_refusal_code_string(CBM_REFUSAL_THEME_UNKNOWN), "THEME_UNKNOWN");
    PASS();
}

SUITE(union_refusal) {
    RUN_TEST(test_refusal_code_roundtrip);
    RUN_TEST(test_refusal_unknown_string_not_a_code);
    RUN_TEST(test_refusal_every_code_has_obligation);
    RUN_TEST(test_retry_identical_detected);
    RUN_TEST(test_retry_different_payload_ok);
    RUN_TEST(test_retry_cross_horizon_ok);
    RUN_TEST(test_retry_different_code_ok);
    RUN_TEST(test_ledger_capacity_bounded);
    RUN_TEST(test_fingerprint_part_separation);
    RUN_TEST(test_track_d_refusal_codes_exist);
}

