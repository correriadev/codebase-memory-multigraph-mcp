/*
 * test_union_ledger.c — Scope A06 acceptance criteria.
 */
#include "test_framework.h"
#include "../src/union/union_ledger.h"

TEST(test_ledger_debit_and_exhaustion) {
    CbmHorizonBudgetLimits limits;
    limits.max_attempts = 3;
    limits.max_time_sec = 60;
    limits.max_tokens = 1000;
    limits.max_irreversible_actions = 2;

    CbmHorizonLedger ledger;
    cbm_ledger_init(&ledger, "h_1", &limits);

    /* Debit attempts */
    ASSERT_EQ(cbm_ledger_debit_attempt(&ledger), CBM_REFUSAL_OK);
    ASSERT_EQ(cbm_ledger_debit_attempt(&ledger), CBM_REFUSAL_OK);
    ASSERT_EQ(cbm_ledger_debit_attempt(&ledger), CBM_REFUSAL_OK);
    /* 4th attempt exceeds max_attempts */
    ASSERT_EQ(cbm_ledger_debit_attempt(&ledger), CBM_REFUSAL_BUDGET_EXHAUSTED);
    ASSERT_TRUE(ledger.exhausted);
    /* AC3: Exhaustion never promotes */
    ASSERT_FALSE(cbm_ledger_can_promote(&ledger));
    PASS();
}

TEST(test_ledger_tokens_and_time_exhaustion) {
    CbmHorizonBudgetLimits limits;
    limits.max_attempts = 10;
    limits.max_time_sec = 100;
    limits.max_tokens = 500;
    limits.max_irreversible_actions = 5;

    CbmHorizonLedger ledger;
    cbm_ledger_init(&ledger, "h_2", &limits);

    ASSERT_EQ(cbm_ledger_debit_tokens(&ledger, 300), CBM_REFUSAL_OK);
    ASSERT_EQ(cbm_ledger_debit_tokens(&ledger, 300), CBM_REFUSAL_BUDGET_EXHAUSTED);
    ASSERT_TRUE(ledger.exhausted);
    ASSERT_FALSE(cbm_ledger_can_promote(&ledger));
    PASS();
}

TEST(test_ledger_raise_budget_resumes) {
    CbmHorizonBudgetLimits limits;
    limits.max_attempts = 2;
    limits.max_time_sec = 10;
    limits.max_tokens = 100;
    limits.max_irreversible_actions = 1;

    CbmHorizonLedger ledger;
    cbm_ledger_init(&ledger, "h_3", &limits);

    /* Exhaust attempts */
    cbm_ledger_debit_attempt(&ledger);
    cbm_ledger_debit_attempt(&ledger);
    ASSERT_EQ(cbm_ledger_debit_attempt(&ledger), CBM_REFUSAL_BUDGET_EXHAUSTED);
    ASSERT_TRUE(ledger.exhausted);

    /* Operator explicitly raises budget with provenance */
    CbmHorizonBudgetLimits raised = limits;
    raised.max_attempts = 10;
    ASSERT_TRUE(cbm_ledger_raise_budget(&ledger, &raised, "operator_root", "ticket_42"));
    ASSERT_FALSE(ledger.exhausted);
    ASSERT_TRUE(cbm_ledger_can_promote(&ledger));
    ASSERT_EQ(ledger.escalation_count, 1);
    ASSERT_STR_EQ(ledger.last_escalation_operator, "operator_root");
    ASSERT_STR_EQ(ledger.last_escalation_provenance, "ticket_42");

    /* Can debit again */
    ASSERT_EQ(cbm_ledger_debit_attempt(&ledger), CBM_REFUSAL_OK);
    PASS();
}

SUITE(union_ledger) {
    RUN_TEST(test_ledger_debit_and_exhaustion);
    RUN_TEST(test_ledger_tokens_and_time_exhaustion);
    RUN_TEST(test_ledger_raise_budget_resumes);
}
