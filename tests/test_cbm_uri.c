#include "test_framework.h"
#include "../src/core/cbm_uri.h"

TEST(test_cbm_uri_parse_valid) {
    const char *raw = "cbm://repo/pkg/auth/token.go#ValidateToken";
    CbmUri uri;
    int rc = cbm_uri_parse(raw, &uri);
    ASSERT_EQ(rc, CBM_URI_OK);
    ASSERT_STR_EQ(uri.repo, "repo");
    ASSERT_STR_EQ(uri.path, "pkg/auth/token.go");
    ASSERT_STR_EQ(uri.symbol, "ValidateToken");
    ASSERT_GT(uri.hash, 0);

    char buf[512];
    cbm_uri_to_string(&uri, buf, sizeof(buf));
    ASSERT_STR_EQ(buf, raw);
    PASS();
}

TEST(test_cbm_uri_reject_missing_fragment) {
    const char *raw = "cbm://repo/pkg/auth/token.go";
    CbmUri uri;
    int rc = cbm_uri_parse(raw, &uri);
    ASSERT_EQ(rc, CBM_URI_MALFORMED_SYNTAX);
    PASS();
}

TEST(test_cbm_uri_reject_invalid_scheme) {
    const char *raw = "http://repo/pkg/auth/token.go#ValidateToken";
    CbmUri uri;
    int rc = cbm_uri_parse(raw, &uri);
    ASSERT_EQ(rc, CBM_URI_MALFORMED_SYNTAX);
    PASS();
}

TEST(test_cbm_uri_fnv1a_deterministic) {
    const char *str = "cbm://myrepo/src/main.c#main";
    uint64_t h1 = cbm_fnv1a_64(str, strlen(str));
    uint64_t h2 = cbm_fnv1a_64(str, strlen(str));
    ASSERT_EQ(h1, h2);
    ASSERT_GT(h1, 0);
    PASS();
}

SUITE(cbm_uri_suite) {
    RUN_TEST(test_cbm_uri_parse_valid);
    RUN_TEST(test_cbm_uri_reject_missing_fragment);
    RUN_TEST(test_cbm_uri_reject_invalid_scheme);
    RUN_TEST(test_cbm_uri_fnv1a_deterministic);
}
