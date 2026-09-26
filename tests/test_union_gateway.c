/*
 * test_union_gateway.c — Scope A05 acceptance criteria.
 */
#include "test_framework.h"
#include "../src/union/union_gateway.h"

#include <string.h>

TEST(test_gateway_unclassified_blocked) {
    CbmGateway gw;
    cbm_gateway_init(&gw);

    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    char reason[128] = {0};

    CbmGatewayDecision d = cbm_gateway_authorize_action(&gw, "h_1", "skill_a", NULL,
                                                      "dangerous_op", CBM_EFFECT_UNCLASSIFIED,
                                                      NULL, "seq_1", 1000, NULL,
                                                      &refusal, reason, sizeof(reason));
    ASSERT_EQ(d, CBM_GATEWAY_BLOCK);
    ASSERT_EQ(refusal, CBM_REFUSAL_TOOL_UNCLASSIFIED);
    ASSERT_STR_EQ(reason, "unclassified_action:dangerous_op");
    PASS();
}

TEST(test_gateway_idempotent_allowed) {
    CbmGateway gw;
    cbm_gateway_init(&gw);

    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    char reason[128] = {0};

    CbmGatewayDecision d = cbm_gateway_authorize_action(&gw, "h_1", "skill_a", NULL,
                                                      "read_symbol", CBM_EFFECT_IDEMPOTENT,
                                                      NULL, "seq_1", 1000, NULL,
                                                      &refusal, reason, sizeof(reason));
    ASSERT_EQ(d, CBM_GATEWAY_ALLOW);
    ASSERT_EQ(refusal, CBM_REFUSAL_OK);
    PASS();
}

TEST(test_gateway_irreversible_requires_auth_single_use) {
    CbmGateway gw;
    cbm_gateway_init(&gw);

    CbmScopedAuthorization auth;
    memset(&auth, 0, sizeof(auth));
    snprintf(auth.auth_id, sizeof(auth.auth_id), "auth_42");
    snprintf(auth.target_action, sizeof(auth.target_action), "drop_table");
    auth.valid_until_unix = 2000;
    snprintf(auth.snapshot_seq, sizeof(auth.snapshot_seq), "seq_10");

    ASSERT_TRUE(cbm_gateway_register_authorization(&gw, &auth));

    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    char reason[128] = {0};

    /* First execution: valid auth -> allowed */
    CbmGatewayDecision d = cbm_gateway_authorize_action(&gw, "h_1", "skill_a", NULL,
                                                      "drop_table", CBM_EFFECT_IRREVERSIBLE,
                                                      "auth_42", "seq_10", 1500, NULL,
                                                      &refusal, reason, sizeof(reason));
    ASSERT_EQ(d, CBM_GATEWAY_ALLOW);
    ASSERT_EQ(refusal, CBM_REFUSAL_OK);

    /* Second execution (replay test): single-use enforced -> blocked */
    d = cbm_gateway_authorize_action(&gw, "h_1", "skill_a", NULL,
                                    "drop_table", CBM_EFFECT_IRREVERSIBLE,
                                    "auth_42", "seq_10", 1550, NULL,
                                    &refusal, reason, sizeof(reason));
    ASSERT_EQ(d, CBM_GATEWAY_BLOCK);
    ASSERT_EQ(refusal, CBM_REFUSAL_RETRY_IDENTICAL);
    PASS();
}

TEST(test_gateway_authorization_expired_or_stale) {
    CbmGateway gw;
    cbm_gateway_init(&gw);

    CbmScopedAuthorization auth;
    memset(&auth, 0, sizeof(auth));
    snprintf(auth.auth_id, sizeof(auth.auth_id), "auth_expired");
    snprintf(auth.target_action, sizeof(auth.target_action), "commit_changes");
    auth.valid_until_unix = 1000;
    snprintf(auth.snapshot_seq, sizeof(auth.snapshot_seq), "seq_10");

    ASSERT_TRUE(cbm_gateway_register_authorization(&gw, &auth));

    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    char reason[128] = {0};

    /* Expired TTL test */
    CbmGatewayDecision d = cbm_gateway_authorize_action(&gw, "h_1", "skill_a", NULL,
                                                      "commit_changes", CBM_EFFECT_IRREVERSIBLE,
                                                      "auth_expired", "seq_10", 1001, NULL,
                                                      &refusal, reason, sizeof(reason));
    ASSERT_EQ(d, CBM_GATEWAY_BLOCK);
    ASSERT_EQ(refusal, CBM_REFUSAL_SCOPE_EXCEEDED);

    /* Stale sequence test */
    auth.valid_until_unix = 5000;
    snprintf(auth.auth_id, sizeof(auth.auth_id), "auth_stale");
    ASSERT_TRUE(cbm_gateway_register_authorization(&gw, &auth));

    d = cbm_gateway_authorize_action(&gw, "h_1", "skill_a", NULL,
                                    "commit_changes", CBM_EFFECT_IRREVERSIBLE,
                                    "auth_stale", "seq_11", 2000, NULL,
                                    &refusal, reason, sizeof(reason));
    ASSERT_EQ(d, CBM_GATEWAY_BLOCK);
    ASSERT_EQ(refusal, CBM_REFUSAL_STALE_BASE);
    PASS();
}

TEST(test_gateway_compensable_idempotency_and_compensation) {
    CbmGateway gw;
    cbm_gateway_init(&gw);

    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    char reason[128] = {0};

    /* First call */
    CbmGatewayDecision d = cbm_gateway_authorize_action(&gw, "h_1", "skill_a", NULL,
                                                      "reserve_seat", CBM_EFFECT_COMPENSABLE,
                                                      NULL, "seq_1", 1000, "idem_123",
                                                      &refusal, reason, sizeof(reason));
    ASSERT_EQ(d, CBM_GATEWAY_ALLOW);

    /* Replay with same idempotency key */
    d = cbm_gateway_authorize_action(&gw, "h_1", "skill_a", NULL,
                                    "reserve_seat", CBM_EFFECT_COMPENSABLE,
                                    NULL, "seq_1", 1010, "idem_123",
                                    &refusal, reason, sizeof(reason));
    ASSERT_EQ(d, CBM_GATEWAY_ALLOW);

    /* Record compensation */
    ASSERT_TRUE(cbm_gateway_record_compensation(&gw, "h_1", "idem_123"));
    ASSERT_FALSE(cbm_gateway_record_compensation(&gw, "h_1", "non_existent_key"));
    PASS();
}

SUITE(union_gateway) {
    RUN_TEST(test_gateway_unclassified_blocked);
    RUN_TEST(test_gateway_idempotent_allowed);
    RUN_TEST(test_gateway_irreversible_requires_auth_single_use);
    RUN_TEST(test_gateway_authorization_expired_or_stale);
    RUN_TEST(test_gateway_compensable_idempotency_and_compensation);
}
