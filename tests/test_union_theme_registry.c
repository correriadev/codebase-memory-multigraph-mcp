/*
 * test_union_theme_registry.c — Scope D01 acceptance criteria.
 *
 * Verified by host log / return value, never by narrator self-report.
 */
#include "test_framework.h"
#include "../src/union/union_theme_registry.h"
#include "../src/union/union_refusal.h"
#include "../src/foundation/compat.h"
#include "../src/foundation/compat_fs.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

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

TEST(test_theme_search_matches_all_terms_and_pages) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);

    CbmThemeEntry design, testing;
    populate_valid_theme(&design);
    strncpy(design.theme_id, "@public/ui-design-practices", sizeof(design.theme_id) - 1);
    strncpy(design.name, "Interface Design Practices", sizeof(design.name) - 1);
    strncpy(design.namespace, "design", sizeof(design.namespace) - 1);
    strncpy(design.description, "Spacing tokens typography palette motion and component behavior",
            sizeof(design.description) - 1);
    strncpy(design.aliases, "visual hierarchy layout", sizeof(design.aliases) - 1);
    strncpy(design.tags, "design-system interface", sizeof(design.tags) - 1);

    populate_valid_theme(&testing);
    strncpy(testing.theme_id, "@public/react-testing", sizeof(testing.theme_id) - 1);
    strncpy(testing.name, "React Testing Practices", sizeof(testing.name) - 1);
    strncpy(testing.namespace, "testing", sizeof(testing.namespace) - 1);
    strncpy(testing.description, "Test-driven development for React components",
            sizeof(testing.description) - 1);
    strncpy(testing.tags, "frontend tdd", sizeof(testing.tags) - 1);

    ASSERT_EQ(cbm_theme_registry_register(&reg, &design, NULL, 0), CBM_REFUSAL_OK);
    ASSERT_EQ(cbm_theme_registry_register(&reg, &testing, NULL, 0), CBM_REFUSAL_OK);

    CbmThemeSearchHit hits[4];
    size_t total = 0, count = 0;
    ASSERT_EQ(cbm_theme_registry_search(&reg, "spacing typography", NULL, "ACTIVE", 0, 4,
                                        hits, 4, &total, &count), CBM_REFUSAL_OK);
    ASSERT_EQ(total, 1);
    ASSERT_EQ(count, 1);
    ASSERT_STR_EQ(hits[0].entry.theme_id, "@public/ui-design-practices");

    ASSERT_EQ(cbm_theme_registry_search(&reg, "", NULL, NULL, 1, 1, hits, 4, &total, &count),
              CBM_REFUSAL_OK);
    ASSERT_EQ(total, 2);
    ASSERT_EQ(count, 1);

    PASS();
}

TEST(test_theme_versions_are_immutable_and_pinnable) {
    CbmThemeRegistry reg;
    cbm_theme_registry_init(&reg);
    CbmThemeEntry entry;
    populate_valid_theme(&entry);
    strncpy(entry.description, "Spacing scale v1", sizeof(entry.description) - 1);
    strncpy(entry.target_generation, "u0123456789abcdefg1", sizeof(entry.target_generation) - 1);
    ASSERT_EQ(cbm_theme_registry_register(&reg, &entry, NULL, 0), CBM_REFUSAL_OK);

    CbmThemeEntry changed = entry;
    strncpy(changed.description, "Changed content under the same version", sizeof(changed.description) - 1);
    ASSERT_EQ(cbm_theme_registry_register(&reg, &changed, NULL, 0),
              CBM_REFUSAL_THEME_VERSION_IMMUTABLE);

    changed = entry;
    strncpy(changed.target_generation, "u0123456789abcdefg2", sizeof(changed.target_generation) - 1);
    ASSERT_EQ(cbm_theme_registry_register(&reg, &changed, NULL, 0),
              CBM_REFUSAL_THEME_VERSION_IMMUTABLE);

    changed = entry;
    strncpy(changed.version, "1.3.0", sizeof(changed.version) - 1);
    strncpy(changed.description, "Spacing scale v2", sizeof(changed.description) - 1);
    ASSERT_EQ(cbm_theme_registry_register(&reg, &changed, NULL, 0), CBM_REFUSAL_OK);

    CbmThemeEntry pinned, latest;
    ASSERT_EQ(cbm_theme_registry_lookup_version(&reg, entry.theme_id, "1.2.0", &pinned),
              CBM_REFUSAL_OK);
    ASSERT_STR_EQ(pinned.description, "Spacing scale v1");
    ASSERT_EQ(cbm_theme_registry_lookup(&reg, entry.theme_id, &latest), CBM_REFUSAL_OK);
    ASSERT_STR_EQ(latest.version, "1.3.0");
    ASSERT_STR_EQ(latest.description, "Spacing scale v2");

    PASS();
}

TEST(test_theme_catalog_survives_reopen) {
    char path[CBM_THEME_STORE_PATH_MAX];
    char tmp_path[CBM_THEME_STORE_PATH_MAX + 8];
    int written = snprintf(path, sizeof(path), "%s/cbm-theme-registry-test-%llu.json", cbm_tmpdir(),
                           (unsigned long long)time(NULL));
    ASSERT_TRUE(written > 0 && (size_t)written < sizeof(path));
    written = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    ASSERT_TRUE(written > 0 && (size_t)written < sizeof(tmp_path));
    (void)cbm_unlink(path);
    (void)cbm_unlink(tmp_path);

    CbmThemeRegistry writer;
    ASSERT_TRUE(cbm_theme_registry_open(&writer, path));
    CbmThemeEntry entry;
    populate_valid_theme(&entry);
    strncpy(entry.description, "Persisted UI spacing and hierarchy rules", sizeof(entry.description) - 1);
    strncpy(entry.aliases, "layout rhythm", sizeof(entry.aliases) - 1);
    ASSERT_EQ(cbm_theme_registry_register(&writer, &entry, NULL, 0), CBM_REFUSAL_OK);

    CbmThemeRegistry reader;
    cbm_theme_registry_init(&reader);
    ASSERT_TRUE(cbm_theme_registry_open(&reader, path));
    CbmThemeEntry recovered;
    ASSERT_EQ(cbm_theme_registry_lookup_version(&reader, entry.theme_id, entry.version, &recovered),
              CBM_REFUSAL_OK);
    ASSERT_STR_EQ(recovered.description, entry.description);
    ASSERT_STR_EQ(recovered.aliases, entry.aliases);
    ASSERT_STR_EQ(recovered.target_generation, entry.target_generation);

    ASSERT_EQ(cbm_unlink(path), 0);
    (void)cbm_unlink(tmp_path);
    PASS();
}

TEST(test_two_stale_catalog_writers_preserve_both_updates) {
    char path[CBM_THEME_STORE_PATH_MAX];
    char tmp_path[CBM_THEME_STORE_PATH_MAX + 8];
    int written = snprintf(path, sizeof(path), "%s/cbm-theme-registry-writers-%llu.json", cbm_tmpdir(),
                           (unsigned long long)time(NULL));
    ASSERT_TRUE(written > 0 && (size_t)written < sizeof(path));
    written = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    ASSERT_TRUE(written > 0 && (size_t)written < sizeof(tmp_path));
    (void)cbm_unlink(path);
    (void)cbm_unlink(tmp_path);

    CbmThemeRegistry writer_a, writer_b;
    ASSERT_TRUE(cbm_theme_registry_open(&writer_a, path));
    ASSERT_TRUE(cbm_theme_registry_open(&writer_b, path));
    CbmThemeEntry a, b;
    populate_valid_theme(&a);
    populate_valid_theme(&b);
    strncpy(a.theme_id, "@org/writer-a", sizeof(a.theme_id) - 1);
    strncpy(b.theme_id, "@org/writer-b", sizeof(b.theme_id) - 1);
    char lock_path[CBM_THEME_STORE_PATH_MAX + 8];
    snprintf(lock_path,sizeof(lock_path),"%s.lock",path);
    ASSERT_EQ(cbm_mkdir(lock_path),0);
    ASSERT_EQ(cbm_theme_registry_register(&writer_a,&a,NULL,0),CBM_REFUSAL_THEME_PERSISTENCE_FAILED);
    ASSERT_EQ(cbm_rmdir(lock_path),0);
    ASSERT_EQ(cbm_theme_registry_register(&writer_a, &a, NULL, 0), CBM_REFUSAL_OK);
    ASSERT_EQ(cbm_theme_registry_register(&writer_b, &b, NULL, 0), CBM_REFUSAL_OK);

    CbmThemeRegistry reader;
    cbm_theme_registry_init(&reader);
    ASSERT_TRUE(cbm_theme_registry_open(&reader, path));
    CbmThemeEntry recovered;
    ASSERT_EQ(cbm_theme_registry_lookup(&reader, a.theme_id, &recovered), CBM_REFUSAL_OK);
    ASSERT_EQ(cbm_theme_registry_lookup(&reader, b.theme_id, &recovered), CBM_REFUSAL_OK);
    ASSERT_EQ(cbm_unlink(path), 0);
    (void)cbm_unlink(tmp_path);
    PASS();
}

SUITE(union_theme_registry) {
    RUN_TEST(test_theme_schema_validation);
    RUN_TEST(test_theme_absence_is_queryable_state);
    RUN_TEST(test_unregistered_theme_lookup_unknown);
    RUN_TEST(test_valid_theme_registration_and_lookup);
    RUN_TEST(test_namespace_querying);
    RUN_TEST(test_theme_search_matches_all_terms_and_pages);
    RUN_TEST(test_theme_versions_are_immutable_and_pinnable);
    RUN_TEST(test_theme_catalog_survives_reopen);
    RUN_TEST(test_two_stale_catalog_writers_preserve_both_updates);
}
