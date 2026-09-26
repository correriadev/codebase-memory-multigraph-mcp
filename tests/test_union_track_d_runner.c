/*
 * test_union_track_d_runner.c — Standalone test runner for Track D (and Track A refusal extensions).
 */
#include "test_framework.h"

int tf_pass_count = 0;
int tf_fail_count = 0;
int tf_skip_count = 0;

__attribute__((weak)) void suite_union_refusal(void) {}
__attribute__((weak)) void suite_union_theme_registry(void) {}
__attribute__((weak)) void suite_union_binding(void) {}
__attribute__((weak)) void suite_union_routing(void) {}
__attribute__((weak)) void suite_union_cross_territory(void) {}
__attribute__((weak)) void suite_union_theme_drift(void) {}
__attribute__((weak)) void suite_union_founding(void) {}

int main(int argc, char **argv) {
    const char *target = (argc > 1) ? argv[1] : "all";

    printf("\n=== Running Track D Union Test Suites (target=%s) ===\n\n", target);

    if (strcmp(target, "all") == 0 || strcmp(target, "union_refusal") == 0) {
        RUN_SUITE(union_refusal);
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "union_theme_registry") == 0) {
        RUN_SUITE(union_theme_registry);
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "union_binding") == 0) {
        RUN_SUITE(union_binding);
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "union_routing") == 0) {
        RUN_SUITE(union_routing);
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "union_cross_territory") == 0) {
        RUN_SUITE(union_cross_territory);
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "union_theme_drift") == 0) {
        RUN_SUITE(union_theme_drift);
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "union_founding") == 0) {
        RUN_SUITE(union_founding);
    }

    TEST_SUMMARY();
    return tf_fail_count > 0 ? 1 : 0;
}
