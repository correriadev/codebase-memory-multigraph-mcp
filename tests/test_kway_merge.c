#include "test_framework.h"
#include "../src/query/kway_merge.h"

TEST(test_kway_merge_pagination) {
    KWayMergeContext ctx;
    int rc = cbm_kway_merge_init(&ctx, 10, 5);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(ctx.skip, 10);
    ASSERT_EQ(ctx.limit, 5);
    PASS();
}

TEST(test_kway_merge_empty_heap_step_zeroes_out_record) {
    KWayMergeContext ctx;
    cbm_kway_merge_init(&ctx, 0, 100);
    MergeRecord rec;
    memset(&rec, 'A', sizeof(rec)); /* Fill with dirty bytes */
    bool has_more = true;

    int rc = cbm_kway_merge_step(&ctx, &rec, &has_more);
    ASSERT_EQ(rc, 0);
    ASSERT_FALSE(has_more);
    ASSERT_EQ(rec.key[0], '\0');
    cbm_kway_merge_close(&ctx);
    PASS();
}

SUITE(kway_merge_suite) {
    RUN_TEST(test_kway_merge_pagination);
    RUN_TEST(test_kway_merge_empty_heap_step_zeroes_out_record);
}

