/*
 * test_union_trace.c — Scope A09 acceptance criteria.
 */
#include "test_framework.h"
#include "../src/union/union_trace.h"

#include <string.h>

TEST(test_trace_evaluator_json_quarantines_narrative) {
    CbmSessionTrace trace;
    cbm_trace_init(&trace, "h_session_1", "graph_grounding", "contract_grounding");
    snprintf(trace.based_on_seq, sizeof(trace.based_on_seq), "seq_100");
    snprintf(trace.quarantined_narrative, sizeof(trace.quarantined_narrative),
             "Secret narrative that must NEVER appear in evaluator JSON");

    cbm_trace_record_refusal(&trace, CBM_REFUSAL_STALE_BASE);

    CbmHorizonLedger ledger;
    cbm_ledger_init(&ledger, "h_session_1", NULL);
    cbm_ledger_debit_action(&ledger, CBM_EFFECT_IDEMPOTENT);
    cbm_ledger_debit_action(&ledger, CBM_EFFECT_COMPENSABLE);
    cbm_trace_reconcile_ledger(&trace, &ledger);

    CbmPromotionProposal prop;
    memset(&prop, 0, sizeof(prop));
    prop.exclusions.declared = true;
    prop.exclusions.counts[CBM_EXCLUSION_OUT_OF_SCOPE] = 2;
    cbm_trace_reconcile_promotion(&trace, &prop, "COMPLETED");

    char json_buf[1024] = {0};
    ASSERT_EQ(cbm_trace_format_evaluator_json(&trace, json_buf, sizeof(json_buf)), 0);

    /* AC3: Quarantined narrative is NOT in evaluator JSON */
    ASSERT_NULL(strstr(json_buf, "Secret narrative"));
    ASSERT_NOT_NULL(strstr(json_buf, "\"horizon_id\":\"h_session_1\""));
    ASSERT_NOT_NULL(strstr(json_buf, "\"outcome\":\"COMPLETED\""));
    ASSERT_NOT_NULL(strstr(json_buf, "\"idempotent\":1"));
    ASSERT_NOT_NULL(strstr(json_buf, "\"compensable\":1"));
    ASSERT_NOT_NULL(strstr(json_buf, "\"out_of_scope\":2"));
    PASS();
}

SUITE(union_trace) {
    RUN_TEST(test_trace_evaluator_json_quarantines_narrative);
}
