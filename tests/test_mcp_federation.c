#include "test_framework.h"
#include "../src/core/horizon_pool.h"

extern int cbm_mcp_parse_active_horizons(const char *args_json, char horizons[][CBM_HORIZON_ID_MAX], size_t max_horizons, size_t *out_count);

TEST(test_parse_active_horizons_empty) {
    char horizons[4][CBM_HORIZON_ID_MAX];
    size_t count = 999;
    int rc = cbm_mcp_parse_active_horizons("{}", horizons, 4, &count);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(count, 0);
    PASS();
}

TEST(test_parse_active_horizons_populated) {
    char horizons[4][CBM_HORIZON_ID_MAX];
    size_t count = 0;
    const char *json = "{\"query\":\"MATCH (n) RETURN n\",\"active_horizons\":[\"h_test_1\",\"h_test_2\"]}";
    int rc = cbm_mcp_parse_active_horizons(json, horizons, 4, &count);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(count, 2);
    ASSERT_STR_EQ(horizons[0], "h_test_1");
    ASSERT_STR_EQ(horizons[1], "h_test_2");
    PASS();
}

TEST(test_parse_active_horizons_substring_in_query_no_false_positive) {
    char horizons[4][CBM_HORIZON_ID_MAX];
    size_t count = 999;
    const char *json = "{\"query\":\"MATCH (n {active_horizons: 'literal'}) RETURN n\"}";
    int rc = cbm_mcp_parse_active_horizons(json, horizons, 4, &count);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(count, 0);
    PASS();
}

SUITE(mcp_federation_suite) {
    RUN_TEST(test_parse_active_horizons_empty);
    RUN_TEST(test_parse_active_horizons_populated);
    RUN_TEST(test_parse_active_horizons_substring_in_query_no_false_positive);
}

