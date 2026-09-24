#include "test_framework.h"
#include "../src/admission/scope_validator.h"
#include "../src/mcp/horizon_sync_handler.h"
#include "../src/core/horizon_pool.h"
#include "../src/foundation/platform.h"
#include <sqlite3.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <yyjson/yyjson.h>

static sqlite3 *create_test_horizon_db(void) {
    sqlite3 *db = NULL;
    int rc = sqlite3_open(":memory:", &db);
    if (rc != SQLITE_OK) return NULL;

    const char *schema =
        "CREATE TABLE IF NOT EXISTS horizon_metadata (\n"
        "    horizon_id TEXT PRIMARY KEY,\n"
        "    client_pid INTEGER NOT NULL,\n"
        "    status TEXT NOT NULL CHECK(status IN ('ACTIVE', 'PROMOTED', 'DISCARDED')),\n"
        "    created_at INTEGER NOT NULL,\n"
        "    last_heartbeat INTEGER NOT NULL,\n"
        "    based_on_seq TEXT NOT NULL DEFAULT '0'\n"
        ");\n"
        "CREATE TABLE IF NOT EXISTS symbolic_nodes (\n"
        "    cbm_uri TEXT PRIMARY KEY,\n"
        "    label TEXT NOT NULL,\n"
        "    epistemic_status TEXT NOT NULL DEFAULT 'PROPOSED',\n"
        "    is_dangling INTEGER NOT NULL DEFAULT 0,\n"
        "    code_snippet TEXT,\n"
        "    created_at INTEGER NOT NULL\n"
        ");\n"
        "CREATE TABLE IF NOT EXISTS virtual_edges (\n"
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "    source_uri TEXT NOT NULL,\n"
        "    target_uri TEXT NOT NULL,\n"
        "    edge_type TEXT NOT NULL,\n"
        "    origin_horizon TEXT NOT NULL,\n"
        "    created_at INTEGER NOT NULL,\n"
        "    UNIQUE(source_uri, target_uri, edge_type)\n"
        ");\n"
        "CREATE VIRTUAL TABLE IF NOT EXISTS spec_fts USING fts5(\n"
        "    file_path UNINDEXED,\n"
        "    heading_slug,\n"
        "    title,\n"
        "    content,\n"
        "    tokenize = 'porter unicode61'\n"
        ");\n";

    char *errmsg = NULL;
    rc = sqlite3_exec(db, schema, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        if (errmsg) sqlite3_free(errmsg);
        sqlite3_close(db);
        return NULL;
    }
    return db;
}

static sqlite3 *create_test_base_db(void) {
    sqlite3 *db = NULL;
    int rc = sqlite3_open(":memory:", &db);
    if (rc != SQLITE_OK) return NULL;

    const char *schema =
        "CREATE TABLE IF NOT EXISTS projects (\n"
        "    name TEXT PRIMARY KEY,\n"
        "    root_dir TEXT NOT NULL\n"
        ");\n"
        "CREATE TABLE IF NOT EXISTS nodes (\n"
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "    project TEXT NOT NULL REFERENCES projects(name),\n"
        "    label TEXT NOT NULL,\n"
        "    name TEXT NOT NULL,\n"
        "    qualified_name TEXT NOT NULL,\n"
        "    file_path TEXT DEFAULT '',\n"
        "    start_line INTEGER DEFAULT 0,\n"
        "    end_line INTEGER DEFAULT 0,\n"
        "    properties TEXT DEFAULT '{}',\n"
        "    UNIQUE(project, qualified_name)\n"
        ");\n"
        "INSERT INTO projects (name, root_dir) VALUES ('test_project', '/test');\n";

    char *errmsg = NULL;
    rc = sqlite3_exec(db, schema, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        if (errmsg) sqlite3_free(errmsg);
        sqlite3_close(db);
        return NULL;
    }
    return db;
}

TEST(test_scope_validator_valid_graph) {
    sqlite3 *hdb = create_test_horizon_db();
    ASSERT_NOT_NULL(hdb);

    /* Insert two nodes connected by a virtual edge */
    sqlite3_exec(hdb,
        "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, is_dangling, code_snippet, created_at) "
        "VALUES ('cbm://repo/src/a.c#ServiceA', 'DomainService', 'PROPOSED', 1, 'void a();', 100);",
        NULL, NULL, NULL);

    sqlite3_exec(hdb,
        "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, is_dangling, code_snippet, created_at) "
        "VALUES ('cbm://repo/src/b.c#ServiceB', 'DomainService', 'PROPOSED', 1, 'void b();', 100);",
        NULL, NULL, NULL);

    sqlite3_exec(hdb,
        "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) "
        "VALUES ('cbm://repo/src/a.c#ServiceA', 'cbm://repo/src/b.c#ServiceB', 'CALLS', 'h1', 100);",
        NULL, NULL, NULL);

    ScopeValidationReport report;
    char err_buf[256] = {0};
    int rc = cbm_validate_scope_horizon(hdb, NULL, true, &report, err_buf, sizeof(err_buf));

    ASSERT_EQ(rc, 0);
    ASSERT_EQ(report.isolated_nodes_count, 0);
    ASSERT_EQ(report.unresolved_deps_count, 0);

    cbm_scope_validation_report_free(&report);
    sqlite3_close(hdb);
    PASS();
}

TEST(test_scope_validator_isolated_dangling_node) {
    sqlite3 *hdb = create_test_horizon_db();
    ASSERT_NOT_NULL(hdb);

    /* 1. Connected node */
    sqlite3_exec(hdb,
        "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, is_dangling, code_snippet, created_at) "
        "VALUES ('cbm://repo/src/conn.c#ConnNode', 'DomainService', 'PROPOSED', 1, 'void c();', 100);",
        NULL, NULL, NULL);

    /* 2. Isolated node */
    sqlite3_exec(hdb,
        "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, is_dangling, code_snippet, created_at) "
        "VALUES ('cbm://repo/src/iso.c#IsoNode', 'DomainService', 'PROPOSED', 1, 'void iso();', 100);",
        NULL, NULL, NULL);

    /* 3. Section node (should NOT be flagged as isolated dangling node) */
    sqlite3_exec(hdb,
        "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, is_dangling, code_snippet, created_at) "
        "VALUES ('cbm://repo/doc.md#overview', 'Section', 'PROPOSED', 1, '# Overview', 100);",
        NULL, NULL, NULL);

    /* Edge involving ConnNode only */
    sqlite3_exec(hdb,
        "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) "
        "VALUES ('cbm://repo/src/conn.c#ConnNode', 'cbm://repo/src/conn.c#ConnNode', 'CALLS', 'h1', 100);",
        NULL, NULL, NULL);

    /* Test with strict_connectivity = true */
    ScopeValidationReport report;
    char err_buf[256] = {0};
    int rc = cbm_validate_scope_horizon(hdb, NULL, true, &report, err_buf, sizeof(err_buf));

    ASSERT_NEQ(rc, 0);
    ASSERT_EQ(report.isolated_nodes_count, 1);
    ASSERT_NOT_NULL(report.isolated_nodes);
    ASSERT_STR_EQ(report.isolated_nodes[0], "cbm://repo/src/iso.c#IsoNode");
    ASSERT_EQ(report.unresolved_deps_count, 0);

    cbm_scope_validation_report_free(&report);

    /* Test with strict_connectivity = false: isolated nodes ignored */
    err_buf[0] = '\0';
    rc = cbm_validate_scope_horizon(hdb, NULL, false, &report, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(report.isolated_nodes_count, 0);
    ASSERT_EQ(report.unresolved_deps_count, 0);

    cbm_scope_validation_report_free(&report);
    sqlite3_close(hdb);
    PASS();
}

TEST(test_scope_validator_missing_base_dependency) {
    sqlite3 *hdb = create_test_horizon_db();
    sqlite3 *base_db = create_test_base_db();
    ASSERT_NOT_NULL(hdb);
    ASSERT_NOT_NULL(base_db);

    /* Node in horizon */
    sqlite3_exec(hdb,
        "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, is_dangling, code_snippet, created_at) "
        "VALUES ('cbm://repo/src/caller.c#CallerFunc', 'DomainService', 'PROPOSED', 1, 'void call();', 100);",
        NULL, NULL, NULL);

    /* Edge pointing to an external base dependency that does NOT exist in base_db */
    sqlite3_exec(hdb,
        "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) "
        "VALUES ('cbm://repo/src/caller.c#CallerFunc', 'cbm://repo/src/base.c#MissingBaseFunc', 'CALLS', 'h1', 100);",
        NULL, NULL, NULL);

    ScopeValidationReport report;
    char err_buf[256] = {0};
    int rc = cbm_validate_scope_horizon(hdb, base_db, true, &report, err_buf, sizeof(err_buf));

    ASSERT_NEQ(rc, 0);
    ASSERT_EQ(report.isolated_nodes_count, 0);
    ASSERT_EQ(report.unresolved_deps_count, 1);
    ASSERT_NOT_NULL(report.unresolved_deps);
    ASSERT_STR_EQ(report.unresolved_deps[0], "cbm://repo/src/base.c#MissingBaseFunc");

    cbm_scope_validation_report_free(&report);
    sqlite3_close(base_db);
    sqlite3_close(hdb);
    PASS();
}

TEST(test_scope_validator_resolved_base_dependency) {
    sqlite3 *hdb = create_test_horizon_db();
    sqlite3 *base_db = create_test_base_db();
    ASSERT_NOT_NULL(hdb);
    ASSERT_NOT_NULL(base_db);

    /* Insert existing function into base_db */
    sqlite3_exec(base_db,
        "INSERT INTO nodes (project, label, name, qualified_name, file_path) "
        "VALUES ('test_project', 'Function', 'ExistingBaseFunc', 'src/base.c:ExistingBaseFunc', 'src/base.c');",
        NULL, NULL, NULL);

    /* Node in horizon */
    sqlite3_exec(hdb,
        "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, is_dangling, code_snippet, created_at) "
        "VALUES ('cbm://repo/src/caller.c#CallerFunc', 'DomainService', 'PROPOSED', 1, 'void call();', 100);",
        NULL, NULL, NULL);

    /* Edge pointing to the existing function */
    sqlite3_exec(hdb,
        "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) "
        "VALUES ('cbm://repo/src/caller.c#CallerFunc', 'cbm://repo/src/base.c#ExistingBaseFunc', 'CALLS', 'h1', 100);",
        NULL, NULL, NULL);

    ScopeValidationReport report;
    char err_buf[256] = {0};
    int rc = cbm_validate_scope_horizon(hdb, base_db, true, &report, err_buf, sizeof(err_buf));

    ASSERT_EQ(rc, 0);
    ASSERT_EQ(report.isolated_nodes_count, 0);
    ASSERT_EQ(report.unresolved_deps_count, 0);

    cbm_scope_validation_report_free(&report);
    sqlite3_close(base_db);
    sqlite3_close(hdb);
    PASS();
}

TEST(test_scope_validator_null_base_db_skips) {
    sqlite3 *hdb = create_test_horizon_db();
    ASSERT_NOT_NULL(hdb);

    sqlite3_exec(hdb,
        "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, is_dangling, code_snippet, created_at) "
        "VALUES ('cbm://repo/src/caller.c#CallerFunc', 'DomainService', 'PROPOSED', 1, 'void call();', 100);",
        NULL, NULL, NULL);

    sqlite3_exec(hdb,
        "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) "
        "VALUES ('cbm://repo/src/caller.c#CallerFunc', 'cbm://repo/src/external.c#ExtFunc', 'CALLS', 'h1', 100);",
        NULL, NULL, NULL);

    ScopeValidationReport report;
    char err_buf[256] = {0};
    /* base_db = NULL => skip base check per PBI-08 */
    int rc = cbm_validate_scope_horizon(hdb, NULL, true, &report, err_buf, sizeof(err_buf));

    ASSERT_EQ(rc, 0);
    ASSERT_EQ(report.isolated_nodes_count, 0);
    ASSERT_EQ(report.unresolved_deps_count, 0);

    cbm_scope_validation_report_free(&report);
    sqlite3_close(hdb);
    PASS();
}

TEST(test_scope_validator_error_handling) {
    ScopeValidationReport report;
    char err_buf[256] = {0};

    /* NULL hdb */
    int rc = cbm_validate_scope_horizon(NULL, NULL, true, &report, err_buf, sizeof(err_buf));
    ASSERT_NEQ(rc, 0);
    ASSERT_TRUE(strlen(err_buf) > 0);

    /* NULL report */
    sqlite3 *hdb = create_test_horizon_db();
    ASSERT_NOT_NULL(hdb);
    err_buf[0] = '\0';
    rc = cbm_validate_scope_horizon(hdb, NULL, true, NULL, err_buf, sizeof(err_buf));
    ASSERT_NEQ(rc, 0);
    ASSERT_TRUE(strlen(err_buf) > 0);

    sqlite3_close(hdb);
    PASS();
}

TEST(test_mcp_sync_horizon_spec_and_validate) {
    HorizonConnectionPool pool;
    cbm_horizon_pool_init(&pool, cbm_resolve_cache_dir());

    char h_id[CBM_HORIZON_ID_MAX] = {0};
    int rc = cbm_create_horizon(&pool, 12345, "h_test_sync_val", "0", h_id, sizeof(h_id));
    ASSERT_EQ(rc, 0);

    const char *spec_md =
        "# Order Management\n\n"
        "## Order Aggregate\n\n"
        "```tactical-spec\n"
        "nodes:\n"
        "  - symbol: OrderAggregate\n"
        "    type: AggregateRoot\n"
        "    target_path: src/domain/order.c\n"
        "    description: Order root entity\n"
        "  - symbol: PaymentService\n"
        "    type: DomainService\n"
        "    target_path: src/domain/payment.c\n"
        "    description: Handles payments\n"
        "edges:\n"
        "  - source: OrderAggregate\n"
        "    target: PaymentService\n"
        "    type: USES\n"
        "```\n";

    /* Test handle_sync_horizon_spec */
    yyjson_mut_doc *sync_args = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *sync_root = yyjson_mut_obj(sync_args);
    yyjson_mut_doc_set_root(sync_args, sync_root);
    yyjson_mut_obj_add_str(sync_args, sync_root, "horizon_id", h_id);
    yyjson_mut_obj_add_str(sync_args, sync_root, "file_path", "docs/specs/order.md");
    yyjson_mut_obj_add_str(sync_args, sync_root, "content", spec_md);
    yyjson_mut_obj_add_str(sync_args, sync_root, "project", "my_repo");

    char *sync_json = yyjson_mut_write(sync_args, 0, NULL);
    yyjson_mut_doc_free(sync_args);

    char *sync_resp = handle_sync_horizon_spec(NULL, sync_json, &pool);
    free(sync_json);

    ASSERT_NOT_NULL(sync_resp);
    ASSERT_TRUE(strstr(sync_resp, "\"success\":true") != NULL);
    ASSERT_TRUE(strstr(sync_resp, "\"nodes_compiled\":2") != NULL);
    ASSERT_TRUE(strstr(sync_resp, "\"edges_compiled\":1") != NULL);
    free(sync_resp);

    /* Test handle_validate_scope_horizon (both nodes connected -> VALID) */
    yyjson_mut_doc *val_args = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *val_root = yyjson_mut_obj(val_args);
    yyjson_mut_doc_set_root(val_args, val_root);
    yyjson_mut_obj_add_str(val_args, val_root, "horizon_id", h_id);
    yyjson_mut_obj_add_bool(val_args, val_root, "strict_connectivity", true);

    char *val_json = yyjson_mut_write(val_args, 0, NULL);
    yyjson_mut_doc_free(val_args);

    char *val_resp = handle_validate_scope_horizon(NULL, val_json, &pool);
    free(val_json);

    ASSERT_NOT_NULL(val_resp);
    ASSERT_TRUE(strstr(val_resp, "\"success\":true") != NULL);
    ASSERT_TRUE(strstr(val_resp, "\"status\":\"VALID\"") != NULL);
    ASSERT_TRUE(strstr(val_resp, "\"isolated_nodes_count\":0") != NULL);
    free(val_resp);

    cbm_horizon_pool_close_all(&pool);
    PASS();
}

SUITE(scope_validator) {
    RUN_TEST(test_scope_validator_valid_graph);
    RUN_TEST(test_scope_validator_isolated_dangling_node);
    RUN_TEST(test_scope_validator_missing_base_dependency);
    RUN_TEST(test_scope_validator_resolved_base_dependency);
    RUN_TEST(test_scope_validator_null_base_db_skips);
    RUN_TEST(test_scope_validator_error_handling);
    RUN_TEST(test_mcp_sync_horizon_spec_and_validate);
}
