#include "test_framework.h"
#include "../src/core/horizon_spec_parser.h"
#include <sqlite3.h>
#include <string.h>
#include <stdio.h>

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
        "    epistemic_status TEXT NOT NULL DEFAULT 'PROPOSED' CHECK(epistemic_status IN ('PROPOSED', 'ACCEPTED', 'CONTESTED', 'SHADOWED')),\n"
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

static int get_table_count(sqlite3 *db, const char *table, const char *where) {
    char sql[256];
    if (where && where[0]) {
        snprintf(sql, sizeof(sql), "SELECT count(*) FROM %s WHERE %s", table, where);
    } else {
        snprintf(sql, sizeof(sql), "SELECT count(*) FROM %s", table);
    }
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    int count = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

TEST(test_compile_spec_json_block) {
    sqlite3 *db = create_test_horizon_db();
    ASSERT_NOT_NULL(db);

    const char *markdown =
        "# Order Service Specification\n"
        "Specification overview text.\n\n"
        "## 1. Domain Model Architecture\n"
        "Architecture overview discussing OrderAggregate and PaymentService.\n\n"
        "```tactical-spec\n"
        "{\n"
        "  \"domain\": \"order_management\",\n"
        "  \"nodes\": [\n"
        "    {\n"
        "      \"symbol\": \"OrderAggregate\",\n"
        "      \"type\": \"AggregateRoot\",\n"
        "      \"target_path\": \"src/domain/order.py\",\n"
        "      \"description\": \"Handles order lifecycle\",\n"
        "      \"methods\": [\n"
        "        \"submit(): void\",\n"
        "        \"cancel(): void\"\n"
        "      ]\n"
        "    },\n"
        "    {\n"
        "      \"symbol\": \"PaymentService\",\n"
        "      \"type\": \"DomainService\",\n"
        "      \"target_path\": \"src/domain/payment.py\",\n"
        "      \"description\": \"Processes payment charges\"\n"
        "    }\n"
        "  ],\n"
        "  \"edges\": [\n"
        "    {\n"
        "      \"source\": \"OrderAggregate\",\n"
        "      \"target\": \"PaymentService\",\n"
        "      \"type\": \"CALLS\"\n"
        "    }\n"
        "  ]\n"
        "}\n"
        "```\n\n"
        "## 2. Acceptance Criteria\n"
        "All orders must transition to completed status upon payment confirmation.\n";

    TacticalSpecCompileReport report;
    char err_buf[256] = {0};
    int rc = cbm_compile_spec_to_horizon(db, "h_order_scope", "order-repo",
                                         "docs/specs/003-tactical-design.md",
                                         markdown, &report, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(report.nodes_compiled, 2);
    ASSERT_EQ(report.edges_compiled, 1);
    ASSERT_EQ(report.sections_indexed, 3);

    /* Verify symbolic_nodes (2 tactical nodes + 3 section nodes) */
    int sym_nodes = get_table_count(db, "symbolic_nodes", "label != 'Section'");
    ASSERT_EQ(sym_nodes, 2);
    int sec_nodes = get_table_count(db, "symbolic_nodes", "label = 'Section'");
    ASSERT_EQ(sec_nodes, 3);

    /* Verify proposed epistemic_status and dangling */
    int proposed_dangling = get_table_count(db, "symbolic_nodes", "epistemic_status = 'PROPOSED' AND is_dangling = 1");
    ASSERT_EQ(proposed_dangling, 5);

    /* Verify virtual_edges */
    int edges = get_table_count(db, "virtual_edges", NULL);
    ASSERT_EQ(edges, 1);

    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, "SELECT source_uri, target_uri, edge_type, origin_horizon FROM virtual_edges", -1, &stmt, NULL);
    ASSERT_EQ(rc, SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 0), "cbm://order-repo/src/domain/order.py#OrderAggregate");
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 1), "cbm://order-repo/src/domain/payment.py#PaymentService");
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 2), "CALLS");
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 3), "h_order_scope");
    sqlite3_finalize(stmt);

    /* Verify FTS indexing */
    int fts_count = get_table_count(db, "spec_fts", NULL);
    ASSERT_EQ(fts_count, 3);

    rc = sqlite3_prepare_v2(db, "SELECT title FROM spec_fts WHERE spec_fts MATCH 'Criteria'", -1, &stmt, NULL);
    ASSERT_EQ(rc, SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 0), "2. Acceptance Criteria");
    sqlite3_finalize(stmt);

    sqlite3_close(db);
    PASS();
}

TEST(test_compile_spec_yaml_block) {
    sqlite3 *db = create_test_horizon_db();
    ASSERT_NOT_NULL(db);

    const char *markdown =
        "# Tactical Design: Ephemeral Horizon Compiler\n"
        "Strategic and tactical details.\n\n"
        "## 1. Tactical Specification\n"
        "Specification description.\n\n"
        "```tactical-spec\n"
        "domain: ephemeral_horizon_compiler\n"
        "bounded_context: EphemeralHorizonCompiler\n"
        "nodes:\n"
        "  - symbol: HorizonMetadataExtension\n"
        "    type: AggregateRoot\n"
        "    target_path: src/db/horizon_schema.sql\n"
        "    description: \"Extensao da tabela horizon_metadata com based_on_seq\"\n"
        "    methods:\n"
        "      - \"add_based_on_seq_column(): void\"\n"
        "  - symbol: HorizonSpecParser\n"
        "    type: DomainService\n"
        "    target_path: src/core/horizon_spec_parser.c\n"
        "    description: \"Parser C11 de blocos tactical-spec\"\n"
        "    methods:\n"
        "      - \"cbm_compile_spec_to_horizon(): int\"\n"
        "  - symbol: HorizonSyncHandler\n"
        "    type: DomainService\n"
        "    target_path: src/mcp/horizon_sync_handler.c\n"
        "    description: \"MCP Sync Handler\"\n"
        "edges:\n"
        "  - source: HorizonSyncHandler\n"
        "    target: HorizonSpecParser\n"
        "    type: CALLS\n"
        "  - source: HorizonSyncHandler\n"
        "    target: HorizonMetadataExtension\n"
        "    type: USES\n"
        "  - source: HorizonMetadataExtension\n"
        "    target: \"cbm://C-Users-corre-Documents-codebase-memory-mcp/src/core/horizon_pool.c#cbm_create_horizon\"\n"
        "    type: EXTENDS\n"
        "```\n\n"
        "## 2. Ubiquitous Language Glossary\n"
        "Glossary of terms including Cognitive Horizon and Ephemeral Spec FTS.\n";

    TacticalSpecCompileReport report;
    char err_buf[256] = {0};
    int rc = cbm_compile_spec_to_horizon(db, "h_ephemeral_horizon_compiler",
                                         "codebase-memory-mcp",
                                         "docs/specs/003-tactical.md",
                                         markdown, &report, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(report.nodes_compiled, 3);
    ASSERT_EQ(report.edges_compiled, 3);
    ASSERT_EQ(report.sections_indexed, 3);

    /* Verify nodes in symbolic_nodes */
    int nodes = get_table_count(db, "symbolic_nodes", "label != 'Section'");
    ASSERT_EQ(nodes, 3);

    /* Verify methods captured in code_snippet */
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, "SELECT code_snippet FROM symbolic_nodes WHERE cbm_uri LIKE '%#HorizonMetadataExtension'", -1, &stmt, NULL);
    ASSERT_EQ(rc, SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    const char *snip = (const char *)sqlite3_column_text(stmt, 0);
    ASSERT_NOT_NULL(snip);
    ASSERT_TRUE(strstr(snip, "Extensao da tabela") != NULL);
    ASSERT_TRUE(strstr(snip, "add_based_on_seq_column(): void") != NULL);
    sqlite3_finalize(stmt);

    /* Verify external URI in edge target */
    rc = sqlite3_prepare_v2(db, "SELECT target_uri FROM virtual_edges WHERE edge_type = 'EXTENDS'", -1, &stmt, NULL);
    ASSERT_EQ(rc, SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 0), "cbm://C-Users-corre-Documents-codebase-memory-mcp/src/core/horizon_pool.c#cbm_create_horizon");
    sqlite3_finalize(stmt);

    /* Verify FTS search for Glossary */
    rc = sqlite3_prepare_v2(db, "SELECT title FROM spec_fts WHERE spec_fts MATCH 'Glossary'", -1, &stmt, NULL);
    ASSERT_EQ(rc, SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 0), "2. Ubiquitous Language Glossary");
    sqlite3_finalize(stmt);

    sqlite3_close(db);
    PASS();
}

TEST(test_compile_spec_idempotency) {
    sqlite3 *db = create_test_horizon_db();
    ASSERT_NOT_NULL(db);

    const char *markdown_v1 =
        "# Spec Document\n\n"
        "## Section A\n"
        "Content A.\n\n"
        "```tactical-spec\n"
        "nodes:\n"
        "  - symbol: ServiceA\n"
        "    type: DomainService\n"
        "    target_path: src/service_a.c\n"
        "    description: \"Service A initial version\"\n"
        "edges:\n"
        "  - source: ServiceA\n"
        "    target: \"cbm://repo/src/base.c#BaseService\"\n"
        "    type: CALLS\n"
        "```\n\n"
        "## Section B\n"
        "Content B.\n";

    TacticalSpecCompileReport report1;
    int rc = cbm_compile_spec_to_horizon(db, "h_test", "repo", "docs/spec.md",
                                         markdown_v1, &report1, NULL, 0);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(report1.nodes_compiled, 1);
    ASSERT_EQ(report1.edges_compiled, 1);
    ASSERT_EQ(report1.sections_indexed, 3);
    ASSERT_EQ(get_table_count(db, "symbolic_nodes", NULL), 4); /* 1 tactical + 3 sections */
    ASSERT_EQ(get_table_count(db, "virtual_edges", NULL), 1);
    ASSERT_EQ(get_table_count(db, "spec_fts", NULL), 3);

    /* Recompile updated specification on the same horizon database */
    const char *markdown_v2 =
        "# Spec Document (Updated)\n\n"
        "## Section A\n"
        "Content A updated.\n\n"
        "```tactical-spec\n"
        "nodes:\n"
        "  - symbol: ServiceA\n"
        "    type: DomainService\n"
        "    target_path: src/service_a.c\n"
        "    description: \"Service A revised version\"\n"
        "edges:\n"
        "  - source: ServiceA\n"
        "    target: \"cbm://repo/src/base.c#BaseService\"\n"
        "    type: CALLS\n"
        "```\n\n"
        "## Section B\n"
        "Content B updated.\n";

    TacticalSpecCompileReport report2;
    rc = cbm_compile_spec_to_horizon(db, "h_test", "repo", "docs/spec.md",
                                     markdown_v2, &report2, NULL, 0);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(report2.nodes_compiled, 1);
    ASSERT_EQ(report2.edges_compiled, 1);
    ASSERT_EQ(report2.sections_indexed, 3);

    /* Check counts are identical (no duplication!) */
    ASSERT_EQ(get_table_count(db, "symbolic_nodes", NULL), 4);
    ASSERT_EQ(get_table_count(db, "virtual_edges", NULL), 1);
    ASSERT_EQ(get_table_count(db, "spec_fts", NULL), 3);

    /* Check description was updated in place */
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, "SELECT code_snippet FROM symbolic_nodes WHERE cbm_uri LIKE '%#ServiceA'", -1, &stmt, NULL);
    ASSERT_EQ(rc, SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 0), "Service A revised version");
    sqlite3_finalize(stmt);

    sqlite3_close(db);
    PASS();
}

TEST(test_compile_spec_no_tactical_block) {
    sqlite3 *db = create_test_horizon_db();
    ASSERT_NOT_NULL(db);

    const char *markdown =
        "# Problem Space Specification\n\n"
        "## 1. Event Storming\n"
        "Simulated domain events and commands.\n\n"
        "## 2. Glossary\n"
        "Cognitive Horizon definition.\n";

    TacticalSpecCompileReport report;
    int rc = cbm_compile_spec_to_horizon(db, "h_test", "repo", "docs/001.md",
                                         markdown, &report, NULL, 0);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(report.nodes_compiled, 0);
    ASSERT_EQ(report.edges_compiled, 0);
    ASSERT_EQ(report.sections_indexed, 3);

    int fts_count = get_table_count(db, "spec_fts", NULL);
    ASSERT_EQ(fts_count, 3);

    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, "SELECT title FROM spec_fts WHERE spec_fts MATCH 'Storming'", -1, &stmt, NULL);
    ASSERT_EQ(rc, SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 0), "1. Event Storming");
    sqlite3_finalize(stmt);

    sqlite3_close(db);
    PASS();
}

TEST(test_compile_spec_heading_slug_collisions) {
    sqlite3 *db = create_test_horizon_db();
    ASSERT_NOT_NULL(db);

    const char *markdown =
        "# Main Title\n"
        "## Overview\n"
        "Overview 1\n"
        "## Overview\n"
        "Overview 2\n"
        "### Overview\n"
        "Overview 3\n";

    TacticalSpecCompileReport report;
    int rc = cbm_compile_spec_to_horizon(db, "h_test", "repo", "docs/notes.md",
                                         markdown, &report, NULL, 0);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(report.sections_indexed, 4);

    /* Verify all 4 sections were inserted into symbolic_nodes with unique URIs */
    int sec_nodes = get_table_count(db, "symbolic_nodes", "label = 'Section'");
    ASSERT_EQ(sec_nodes, 4);

    sqlite3_close(db);
    PASS();
}

TEST(test_compile_spec_error_handling) {
    TacticalSpecCompileReport report;
    char err_buf[256] = {0};

    /* NULL db */
    int rc = cbm_compile_spec_to_horizon(NULL, "h_test", "repo", "file.md", "# Test", &report, err_buf, sizeof(err_buf));
    ASSERT_NEQ(rc, 0);
    ASSERT_TRUE(strlen(err_buf) > 0);

    /* NULL markdown */
    sqlite3 *db = create_test_horizon_db();
    ASSERT_NOT_NULL(db);
    err_buf[0] = '\0';
    rc = cbm_compile_spec_to_horizon(db, "h_test", "repo", "file.md", NULL, &report, err_buf, sizeof(err_buf));
    ASSERT_NEQ(rc, 0);
    ASSERT_TRUE(strlen(err_buf) > 0);

    sqlite3_close(db);
    PASS();
}

SUITE(horizon_spec_parser) {
    RUN_TEST(test_compile_spec_json_block);
    RUN_TEST(test_compile_spec_yaml_block);
    RUN_TEST(test_compile_spec_idempotency);
    RUN_TEST(test_compile_spec_no_tactical_block);
    RUN_TEST(test_compile_spec_heading_slug_collisions);
    RUN_TEST(test_compile_spec_error_handling);
}
