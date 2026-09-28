/*
 * test_union_workflow_runner.c — Dedicated runner for Track W (Workflow) & Union integration.
 */
#include "test_framework.h"
#include <stdio.h>
#include <string.h>

int tf_pass_count = 0;
int tf_fail_count = 0;
int tf_skip_count = 0;

__attribute__((weak)) void suite_union_workflow_e2e(void) {}
__attribute__((weak)) void suite_union_refusal(void) {}
__attribute__((weak)) void suite_union_session(void) {}
__attribute__((weak)) void suite_union_gateway(void) {}
__attribute__((weak)) void suite_union_routing(void) {}
__attribute__((weak)) void suite_union_theme_registry(void) {}
__attribute__((weak)) void suite_union_binding(void) {}
__attribute__((weak)) void suite_union_founding(void) {}
__attribute__((weak)) void suite_union_contest(void) {}

int main(int argc, char **argv) {
    const char *target = (argc > 1) ? argv[1] : "all";

    printf("\n=== Running Union Workflow & E2E Suites (Track W) ===\n\n");

    if (strcmp(target, "all") == 0 || strcmp(target, "union_workflow_e2e") == 0) {
        RUN_SUITE(union_workflow_e2e);
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "union_session") == 0) {
        RUN_SUITE(union_session);
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "union_gateway") == 0) {
        RUN_SUITE(union_gateway);
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "union_routing") == 0) {
        RUN_SUITE(union_routing);
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "union_theme_registry") == 0) {
        RUN_SUITE(union_theme_registry);
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "union_binding") == 0) {
        RUN_SUITE(union_binding);
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "union_founding") == 0) {
        RUN_SUITE(union_founding);
    }
    if (strcmp(target, "all") == 0 || strcmp(target, "union_contest") == 0) {
        RUN_SUITE(union_contest);
    }

    TEST_SUMMARY();
    return tf_fail_count > 0 ? 1 : 0;
}
