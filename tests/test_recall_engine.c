#include "test_framework.h"
#include "../src/admission/recall_engine.h"

TEST(test_recall_engine_init) {
    CbmUri root;
    cbm_uri_parse("cbm://repo/pkg/auth.go#Token", &root);

    RecallReport report;
    int rc = cbm_trigger_recall(NULL, &root, "Anchor drift detected", &report);
    ASSERT_EQ(rc, 0);
    ASSERT_STR_EQ(report.reason, "Anchor drift detected");
    ASSERT_EQ(report.affected_count, 0);
    PASS();
}

TEST(test_recall_engine_capacity) {
    ASSERT_GE(CBM_RECALL_MAX_AFFECTED, 256);
    PASS();
}

SUITE(recall_engine_suite) {
    RUN_TEST(test_recall_engine_init);
    RUN_TEST(test_recall_engine_capacity);
}

