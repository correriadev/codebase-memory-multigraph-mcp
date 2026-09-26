/*
 * test_union_theme_registry.c — Scope D01 acceptance criteria.
 *
 * Verified by host log / return value, never by narrator self-report.
 */
#include "test_framework.h"
#include "../src/union/union_theme_registry.h"
#include "../src/union/union_refusal.h"

#include <string.h>

static void populate_valid_theme(CbmThemeEntry *entry) {
    memset(entry, 0, sizeof(*entry));
    strncpy(entry->theme_id, "@inst/clean-arch", sizeof(entry->theme_id) - 1);
    strncpy(entry->name, "Clean Architecture Guidelines", sizeof(entry->name) - 1);
    strncpy(entry->namespace, "institutional", sizeof(entry->namespace) - 1);
    strncpy(entry->target_uri, "theme://inst/clean-arch-v1", sizeof(entry->target_uri) - 1);
    strncpy(entry->version, "1.2.0", sizeof(entry->version) - 1);
    strncpy(entry->curator, "arch_guild", sizeof(entry->curator) - 1);
    entry->status = CBM_THEME_ACTIVE;
    strncpy(entry->founding_provenance, "session_genesis_01", sizeof(entry->founding_provenance) - 1);
}

/* AC1: Missing mandatory fields triggers THEME_SCHEMA_INVALID */
TEST(test_theme_schema_validation) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmThemeEntry entry;
    char reason[128];

    /* Missing theme_id */
    populate_valid_theme(&entry);
    entry.theme_id[0] = '\0';
    CbmRefusalCode code = cbm_theme_registry_register(&reg, &entry, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_THEME_SCHEMA_INVALID);
    ASSERT_TRUE(strstr(reason, "theme_id") != NULL);

    /* Missing namespace */
    populate_valid_theme(&entry);
    entry.namespace[0] = '\0';
    code = cbm_theme_registry_register(&reg, &entry, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_THEME_SCHEMA_INVALID);
    ASSERT_TRUE(strstr(reason, "namespace") != NULL);

    /* Missing curator */
    populate_valid_theme(&entry);
    entry.curator[0] = '\0';
    code = cbm_theme_registry_register(&reg, &entry, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_THEME_SCHEMA_INVALID);
    ASSERT_TRUE(strstr(reason, "curator") != NULL);

    /* Missing version */
    populate_valid_theme(&entry);
    entry.version[0] = '\0';
    code = cbm_theme_registry_register(&reg, &entry, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_THEME_SCHEMA_INVALID);
    ASSERT_TRUE(strstr(reason, "version") != NULL);

    PASS();
}

/* AC2: Registered theme with status ABSENT is returned as valid epistemic state */
TEST(test_theme_absence_is_queryable_state) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmThemeEntry entry;
    populate_valid_theme(&entry);
    strncpy(entry.theme_id, "@inst/future-patterns", sizeof(entry.theme_id) - 1);
    entry.status = CBM_THEME_ABSENT; /* unmaterialized graph */
    entry.target_uri[0] = '\0';

    CbmRefusalCode code = cbm_theme_registry_register(&reg, &entry, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    CbmThemeEntry out_entry;
    code = cbm_theme_registry_lookup(&reg, "@inst/future-patterns", &out_entry);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_EQ(out_entry.status, CBM_THEME_ABSENT);
    ASSERT_STR_EQ(out_entry.theme_id, "@inst/future-patterns");

    PASS();
}

/* AC3: Querying an unregistered theme emits THEME_UNKNOWN */
TEST(test_unregistered_theme_lookup_unknown) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmThemeEntry out_entry;
    CbmRefusalCode code = cbm_theme_registry_lookup(&reg, "@inst/non-existent", &out_entry);
    ASSERT_EQ(code, CBM_REFUSAL_THEME_UNKNOWN);

    PASS();
}

/* AC4: Valid active theme registration and retrieval */
TEST(test_valid_theme_registration_and_lookup) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmThemeEntry entry;
    populate_valid_theme(&entry);

    CbmRefusalCode code = cbm_theme_registry_register(&reg, &entry, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    CbmThemeEntry out_entry;
    code = cbm_theme_registry_lookup(&reg, "@inst/clean-arch", &out_entry);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_STR_EQ(out_entry.theme_id, "@inst/clean-arch");
    ASSERT_STR_EQ(out_entry.version, "1.2.0");
    ASSERT_EQ(out_entry.status, CBM_THEME_ACTIVE);

    PASS();
}

/* AC5: Querying themes by namespace */
TEST(test_namespace_querying) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmThemeEntry entry1, entry2, entry3;
    populate_valid_theme(&entry1);
    strncpy(entry1.theme_id, "@inst/frontend", sizeof(entry1.theme_id) - 1);
    strncpy(entry1.namespace, "institutional", sizeof(entry1.namespace) - 1);

    populate_valid_theme(&entry2);
    strncpy(entry2.theme_id, "@inst/backend", sizeof(entry2.theme_id) - 1);
    strncpy(entry2.namespace, "institutional", sizeof(entry2.namespace) - 1);

    populate_valid_theme(&entry3);
    strncpy(entry3.theme_id, "@user/experimental", sizeof(entry3.theme_id) - 1);
    strncpy(entry3.namespace, "personal", sizeof(entry3.namespace) - 1);

    ASSERT_EQ(cbm_theme_registry_register(&reg, &entry1, NULL, 0), CBM_REFUSAL_OK);
    ASSERT_EQ(cbm_theme_registry_register(&reg, &entry2, NULL, 0), CBM_REFUSAL_OK);
    ASSERT_EQ(cbm_theme_registry_register(&reg, &entry3, NULL, 0), CBM_REFUSAL_OK);

    CbmThemeEntry results[4];
    size_t count = 0;
    CbmRefusalCode code = cbm_theme_registry_query_namespace(&reg, "institutional", results, 4, &count);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_EQ(count, 2);

    code = cbm_theme_registry_query_namespace(&reg, "personal", results, 4, &count);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_EQ(count, 1);
    ASSERT_STR_EQ(results[0].theme_id, "@user/experimental");

    PASS();
}

SUITE(union_theme_registry) {
    RUN_TEST(test_theme_schema_validation);
    RUN_TEST(test_theme_absence_is_queryable_state);
    RUN_TEST(test_unregistered_theme_lookup_unknown);
    RUN_TEST(test_valid_theme_registration_and_lookup);
    RUN_TEST(test_namespace_querying);
}
