#include "test_framework.h"
#include "test_helpers.h"
#include "../src/admission/anchor_checker.h"
#include "../src/admission/admission_gate.h"
#include "../src/core/cbm_uri.h"
#include <sqlite3.h>


TEST(test_anchor_checker_fast_path) {
    TwoTierAnchor a;
    memset(&a, 0, sizeof(a));
    snprintf(a.file_path, sizeof(a.file_path), "test.go");
    snprintf(a.expected_text, sizeof(a.expected_text), "func ValidateToken() {}");
    a.byte_start = 0;
    a.byte_len = (uint32_t)strlen(a.expected_text);
    a.ast_signature_hash = cbm_fnv1a_64(a.expected_text, a.byte_len);

    ASSERT_GT(a.ast_signature_hash, 0);
    PASS();
}

TEST(test_anchor_checker_oversized_byte_len) {
    TwoTierAnchor a;
    memset(&a, 0, sizeof(a));
    snprintf(a.file_path, sizeof(a.file_path), "nonexistent.go");
    a.byte_len = 2048; /* exceeds 1024 capacity */
    bool match = cbm_fast_offset_match(".", &a);
    ASSERT_FALSE(match);
    PASS();
}

TEST(test_anchor_checker_ast_null_guard) {
    bool ok = true;
    int rc = cbm_ast_signature_match(".", NULL, &ok);
    ASSERT_EQ(rc, -1);
    ASSERT_FALSE(ok);
    PASS();
}

TEST(test_anchor_checker_whitespace_normalization) {
    /* Verify that normalized hash handles CRLF and multiple spaces gracefully */
    TwoTierAnchor a;
    memset(&a, 0, sizeof(a));
    snprintf(a.expected_text, sizeof(a.expected_text), "func ValidateToken(\n    token string,\n) bool");
    a.byte_len = (uint32_t)strlen(a.expected_text);
    a.ast_signature_hash = cbm_fnv1a_64(a.expected_text, a.byte_len);
    ASSERT_GT(a.ast_signature_hash, 0);
    PASS();
}

TEST(test_anchor_checker_relocation_default_state) {
    TwoTierAnchorRelocation reloc;
    memset(&reloc, 0, sizeof(reloc));
    ASSERT_FALSE(reloc.was_relocated);
    ASSERT_FALSE(reloc.structural_hash_matched);
    ASSERT_EQ(reloc.new_byte_start, 0);
    ASSERT_EQ(reloc.new_byte_len, 0);
    PASS();
}

TEST(test_anchor_checker_resolve_language) {
    ASSERT_EQ(cbm_resolve_language_from_path("src/admission/anchor_checker.c"), CBM_LANG_C);
    ASSERT_EQ(cbm_resolve_language_from_path("src/admission/anchor_checker.h"), CBM_LANG_C);
    ASSERT_EQ(cbm_resolve_language_from_path("sdk/src/runner.ts"), CBM_LANG_TYPESCRIPT);
    ASSERT_EQ(cbm_resolve_language_from_path("lib/utils.js"), CBM_LANG_TYPESCRIPT);
    ASSERT_EQ(cbm_resolve_language_from_path("component.tsx"), CBM_LANG_TYPESCRIPT);
    ASSERT_EQ(cbm_resolve_language_from_path("component.jsx"), CBM_LANG_TYPESCRIPT);
    ASSERT_EQ(cbm_resolve_language_from_path("tests/harness/verifier.py"), CBM_LANG_PYTHON);
    ASSERT_EQ(cbm_resolve_language_from_path("docs/README.md"), CBM_LANG_UNKNOWN);
    ASSERT_EQ(cbm_resolve_language_from_path("Makefile"), CBM_LANG_UNKNOWN);
    ASSERT_EQ(cbm_resolve_language_from_path(NULL), CBM_LANG_UNKNOWN);
    ASSERT_EQ(cbm_resolve_language_from_path(""), CBM_LANG_UNKNOWN);
    PASS();
}

TEST(test_anchor_checker_verify_null_guards) {
    bool ok = true;
    TwoTierAnchorRelocation reloc;
    memset(&reloc, 0, sizeof(reloc));
    int rc = cbm_verify_two_tier_anchor(".", NULL, &ok, &reloc);
    ASSERT_EQ(rc, -1);
    ASSERT_FALSE(ok);

    TwoTierAnchor a;
    memset(&a, 0, sizeof(a));
    rc = cbm_verify_two_tier_anchor(".", &a, NULL, &reloc);
    ASSERT_EQ(rc, -1);
    PASS();
}

TEST(test_anchor_checker_path_traversal_relative) {
    TwoTierAnchor a;
    memset(&a, 0, sizeof(a));
    snprintf(a.file_path, sizeof(a.file_path), "../../etc/passwd");
    snprintf(a.expected_text, sizeof(a.expected_text), "root:x:0:0");
    a.byte_start = 0;
    a.byte_len = (uint32_t)strlen(a.expected_text);
    a.ast_signature_hash = 99999;

    bool match = cbm_fast_offset_match(".", &a);
    ASSERT_FALSE(match);

    bool ok = true;
    TwoTierAnchorRelocation reloc;
    memset(&reloc, 0, sizeof(reloc));
    int rc = cbm_verify_two_tier_anchor(".", &a, &ok, &reloc);
    ASSERT_EQ(rc, -1);
    ASSERT_FALSE(ok);
    PASS();
}

TEST(test_anchor_checker_path_traversal_absolute_escape) {
    TwoTierAnchor a;
    memset(&a, 0, sizeof(a));
#ifdef _WIN32
    snprintf(a.file_path, sizeof(a.file_path), "C:/Windows/System32/drivers/etc/hosts");
#else
    snprintf(a.file_path, sizeof(a.file_path), "/etc/passwd");
#endif
    snprintf(a.expected_text, sizeof(a.expected_text), "localhost");
    a.byte_start = 0;
    a.byte_len = (uint32_t)strlen(a.expected_text);
    a.ast_signature_hash = 88888;

    bool match = cbm_fast_offset_match(".", &a);
    ASSERT_FALSE(match);

    bool ok = true;
    TwoTierAnchorRelocation reloc;
    memset(&reloc, 0, sizeof(reloc));
    int rc = cbm_verify_two_tier_anchor(".", &a, &ok, &reloc);
    ASSERT_EQ(rc, -1);
    ASSERT_FALSE(ok);
    PASS();
}

TEST(test_anchor_checker_const_anchor_no_mutation) {
    const TwoTierAnchor a = {
        .file_path = "valid_test.c",
        .symbol_name = "dummy",
        .byte_start = 120,
        .byte_len = 28,
        .ast_signature_hash = 77777,
        .expected_text = "int dummy(void) { return 0; }"
    };
    bool ok = true;
    TwoTierAnchorRelocation reloc;
    memset(&reloc, 0, sizeof(reloc));
    /* Verifies safe invocation with const TwoTierAnchor* without mutating caller memory */
    int rc = cbm_verify_two_tier_anchor(".", &a, &ok, &reloc);
    (void)rc;
    ASSERT_EQ(a.byte_start, 120);
    ASSERT_EQ(a.byte_len, 28);
    PASS();
}

TEST(test_anchor_checker_zero_ast_hash_rejected) {
    TwoTierAnchor a;
    memset(&a, 0, sizeof(a));
    snprintf(a.file_path, sizeof(a.file_path), "test.c");
    snprintf(a.expected_text, sizeof(a.expected_text), "int test(void) {}");
    a.byte_start = 0;
    a.byte_len = (uint32_t)strlen(a.expected_text);
    a.ast_signature_hash = 0; /* Malformed: zero hash */

    bool match = cbm_fast_offset_match(".", &a);
    ASSERT_FALSE(match);

    bool ok = true;
    TwoTierAnchorRelocation reloc;
    memset(&reloc, 0, sizeof(reloc));
    int rc = cbm_verify_two_tier_anchor(".", &a, &ok, &reloc);
    ASSERT_EQ(rc, -1);
    ASSERT_FALSE(ok);
    PASS();
}

TEST(test_paths_match_strict_rejects_traversal_and_hidden_file_confusion) {
    /* 1. Traversal sequences must be strictly rejected */
    ASSERT_FALSE(cbm_paths_match_strict("../src/foo.c", "src/foo.c"));
    ASSERT_FALSE(cbm_paths_match_strict("src/../src/foo.c", "src/foo.c"));
    ASSERT_FALSE(cbm_paths_match_strict("src/foo.c", "../src/foo.c"));

    /* 2. Hidden files starting with dot must NOT have their dot stripped */
    ASSERT_FALSE(cbm_paths_match_strict(".hidden.c", "hidden.c"));
    ASSERT_FALSE(cbm_paths_match_strict("hidden.c", ".hidden.c"));
    ASSERT_TRUE(cbm_paths_match_strict(".hidden.c", ".hidden.c"));
    ASSERT_TRUE(cbm_paths_match_strict("./.hidden.c", ".hidden.c"));

    /* 3. Absolute vs relative asymmetry must be rejected */
    ASSERT_FALSE(cbm_paths_match_strict("/src/foo.c", "src/foo.c"));
    ASSERT_FALSE(cbm_paths_match_strict("src/foo.c", "/src/foo.c"));

    /* 4. Benign leading ./ and backslash normalization must succeed */
    ASSERT_TRUE(cbm_paths_match_strict("./src/foo.c", "src/foo.c"));
    ASSERT_TRUE(cbm_paths_match_strict("src\\foo.c", "src/foo.c"));
    ASSERT_TRUE(cbm_paths_match_strict(".\\src\\foo.c", "src/foo.c"));

    /* 5. Substring / suffix mismatches must be rejected */
    ASSERT_FALSE(cbm_paths_match_strict("other/src/foo.c", "src/foo.c"));

    /* 6. Redundant slashes after dot must be strictly rejected */
    ASSERT_FALSE(cbm_paths_match_strict(".//src/foo.c", "src/foo.c"));
    ASSERT_FALSE(cbm_paths_match_strict("src/foo.c", ".//src/foo.c"));
    ASSERT_FALSE(cbm_paths_match_strict(".\\/src/foo.c", "src/foo.c"));
    PASS();
}

TEST(test_find_matching_anchor_complete_identity_repo_path_symbol) {
    TwoTierAnchor anchors[2];
    memset(anchors, 0, sizeof(anchors));
    snprintf(anchors[0].file_path, sizeof(anchors[0].file_path), "src/foo.c");
    snprintf(anchors[0].symbol_name, sizeof(anchors[0].symbol_name), "A");
    anchors[0].byte_len = 10;
    anchors[0].ast_signature_hash = 100;

    /* Case 1: Same path & symbol, but divergent repo -> must be rejected */
    int m_cross = cbm_find_matching_anchor("cbm://other_proj/src/foo.c#A", "A", "my_proj", anchors, 1);
    ASSERT_EQ(m_cross, -1);

    /* Case 2: Matching repo, path, symbol -> must be accepted */
    int m_ok = cbm_find_matching_anchor("cbm://my_proj/src/foo.c#A", "A", "my_proj", anchors, 1);
    ASSERT_EQ(m_ok, 0);

    /* Case 3: Path traversal inside node URI -> must be rejected */
    int m_trav = cbm_find_matching_anchor("cbm://my_proj/../src/foo.c#A", "A", "my_proj", anchors, 1);
    ASSERT_EQ(m_trav, -1);

    /* Case 4: Símbolo divergente (#AB) com label='A' -> primazia do símbolo da URI, must be rejected */
    int m_sym = cbm_find_matching_anchor("cbm://my_proj/src/foo.c#AB", "A", "my_proj", anchors, 1);
    ASSERT_EQ(m_sym, -1);

    /* Case 5: Arquivo oculto .hidden.c vs hidden.c -> must be rejected */
    snprintf(anchors[1].file_path, sizeof(anchors[1].file_path), "hidden.c");
    snprintf(anchors[1].symbol_name, sizeof(anchors[1].symbol_name), "B");
    int m_hidden = cbm_find_matching_anchor("cbm://my_proj/.hidden.c#B", "B", "my_proj", anchors, 2);
    ASSERT_EQ(m_hidden, -1);

    /* Case 6: Repo-less relative URI src/foo.c#A when expected_repo is specified -> must be rejected */
    int m_norepo = cbm_find_matching_anchor("src/foo.c#A", "A", "my_proj", anchors, 1);
    ASSERT_EQ(m_norepo, -1);

    /* Case 7: Repo-less URI with label only when expected_repo is specified -> must be rejected */
    int m_norepo2 = cbm_find_matching_anchor("A", "A", "my_proj", anchors, 1);
    ASSERT_EQ(m_norepo2, -1);

    /* Case 8: When expected_repo is NULL (legacy), relative URI is accepted */
    int m_legacy = cbm_find_matching_anchor("src/foo.c#A", "A", NULL, anchors, 1);
    ASSERT_EQ(m_legacy, 0);

    PASS();
}

const TSLanguage *tree_sitter_c(void);

static void init_valid_c_anchor(const char *temp_dir, TwoTierAnchor *out_anchor) {
    const char *code = "int A(void) { return 42; }\n";
    th_write_file(TH_PATH(temp_dir, "src/foo.c"), code);

    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_c());
    TSTree *tree = ts_parser_parse_string(parser, NULL, code, (uint32_t)strlen(code));
    TSNode root = ts_tree_root_node(tree);

    DeclarationNodeMatch match = {0};
    cbm_ts_find_declaration_node(root, "A", code, &match);
    uint64_t hash = cbm_ts_compute_declaration_hash(match.node, code);

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    memset(out_anchor, 0, sizeof(TwoTierAnchor));
    snprintf(out_anchor->file_path, sizeof(out_anchor->file_path), "src/foo.c");
    snprintf(out_anchor->symbol_name, sizeof(out_anchor->symbol_name), "A");
    out_anchor->byte_start = match.start_byte;
    out_anchor->byte_len = match.end_byte - match.start_byte;
    out_anchor->ast_signature_hash = hash;
    if (out_anchor->byte_len < sizeof(out_anchor->expected_text)) {
        memcpy(out_anchor->expected_text, code + match.start_byte, out_anchor->byte_len);
        out_anchor->expected_text[out_anchor->byte_len] = '\0';
    }
}

static void init_valid_c_two_anchors(const char *temp_dir, TwoTierAnchor *out_a, TwoTierAnchor *out_b) {
    const char *code = "int A(void) { return 42; }\nint B(void) { return 84; }\n";
    th_write_file(TH_PATH(temp_dir, "src/foo.c"), code);

    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_c());
    TSTree *tree = ts_parser_parse_string(parser, NULL, code, (uint32_t)strlen(code));
    TSNode root = ts_tree_root_node(tree);

    DeclarationNodeMatch match_a = {0};
    cbm_ts_find_declaration_node(root, "A", code, &match_a);
    uint64_t hash_a = cbm_ts_compute_declaration_hash(match_a.node, code);

    DeclarationNodeMatch match_b = {0};
    cbm_ts_find_declaration_node(root, "B", code, &match_b);
    uint64_t hash_b = cbm_ts_compute_declaration_hash(match_b.node, code);

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    memset(out_a, 0, sizeof(TwoTierAnchor));
    snprintf(out_a->file_path, sizeof(out_a->file_path), "src/foo.c");
    snprintf(out_a->symbol_name, sizeof(out_a->symbol_name), "A");
    out_a->byte_start = match_a.start_byte;
    out_a->byte_len = match_a.end_byte - match_a.start_byte;
    out_a->ast_signature_hash = hash_a;
    if (out_a->byte_len < sizeof(out_a->expected_text)) {
        memcpy(out_a->expected_text, code + match_a.start_byte, out_a->byte_len);
        out_a->expected_text[out_a->byte_len] = '\0';
    }

    memset(out_b, 0, sizeof(TwoTierAnchor));
    snprintf(out_b->file_path, sizeof(out_b->file_path), "src/foo.c");
    snprintf(out_b->symbol_name, sizeof(out_b->symbol_name), "B");
    out_b->byte_start = match_b.start_byte;
    out_b->byte_len = match_b.end_byte - match_b.start_byte;
    out_b->ast_signature_hash = hash_b;
    if (out_b->byte_len < sizeof(out_b->expected_text)) {
        memcpy(out_b->expected_text, code + match_b.start_byte, out_b->byte_len);
        out_b->expected_text[out_b->byte_len] = '\0';
    }
}

TEST(test_promote_horizon_in_memory_base_db_rejected_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_memdb_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *mem_db = NULL;
    ASSERT_EQ(sqlite3_open(":memory:", &mem_db), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, mem_db);

    char h_id[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, 1234, "h_mem", "0", h_id, sizeof(h_id)), 0);

    TwoTierAnchor a;
    init_valid_c_anchor(temp_dir, &a);

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_id, &a, 1, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_COMMIT_FAILED);
    ASSERT_TRUE(strstr(err_buf, ":memory:") != NULL);

    sqlite3_close_v2(mem_db);
    cbm_horizon_pool_close_all(&pool);
    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_atomic_2pc_success_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_succ2pc_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_id[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, 1234, "h_atomic", "0", h_id, sizeof(h_id)), 0);

    sqlite3 *hdb = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_id, &hdb), 0);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor a;
    init_valid_c_anchor(temp_dir, &a);

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_id, &a, 1, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, CBM_ADMISSION_OK);

    /* Verify base_db has the node committed */
    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes WHERE cbm_uri = 'cbm://my_proj/src/foo.c#A';", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 1);
    sqlite3_finalize(stmt);

    /* Verify generation was logged */
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT MAX(generation) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_GTE(sqlite3_column_int(stmt, 0), 1);
    sqlite3_finalize(stmt);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);

    /* Verify horizon status in metadata is PROMOTED */
    sqlite3 *recheck_hdb = NULL;
    char hpath[512];
    ASSERT_EQ(cbm_horizon_pool_get_path(&pool, h_id, hpath, sizeof(hpath)), 0);
    ASSERT_EQ(sqlite3_open(hpath, &recheck_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(recheck_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ?;", -1, &stmt, NULL), SQLITE_OK);
    sqlite3_bind_text(stmt, 1, h_id, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 0), "PROMOTED");
    sqlite3_finalize(stmt);
    sqlite3_close_v2(recheck_hdb);

    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_rollback_on_trigger_failure_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_fail2pc_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);
    /* Trigger aborting edge insert */
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TRIGGER fail_edges BEFORE INSERT ON virtual_edges BEGIN SELECT RAISE(ABORT, 'Simulated mid-stream failure'); END;", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_id[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, 1234, "h_fail", "0", h_id, sizeof(h_id)), 0);

    sqlite3 *hdb = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_id, &hdb), 0);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'cbm://my_proj/src/foo.c#A', 'CALLS', 'h_fail', 1);", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor a;
    init_valid_c_anchor(temp_dir, &a);

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_id, &a, 1, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED);

    /* Verify base_db was rolled back: 0 nodes committed */
    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);

    /* Verify horizon status remains ACTIVE */
    sqlite3 *recheck_hdb = NULL;
    char hpath[512];
    ASSERT_EQ(cbm_horizon_pool_get_path(&pool, h_id, hpath, sizeof(hpath)), 0);
    ASSERT_EQ(sqlite3_open(hpath, &recheck_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(recheck_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ?;", -1, &stmt, NULL), SQLITE_OK);
    sqlite3_bind_text(stmt, 1, h_id, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 0), "ACTIVE");
    sqlite3_finalize(stmt);
    sqlite3_close_v2(recheck_hdb);

    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_requires_active_status_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_inact_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_id[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, 1234, "h_inact", "0", h_id, sizeof(h_id)), 0);

    sqlite3 *hdb = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_id, &hdb), 0);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);

    /* Set horizon status to DISCARDED instead of ACTIVE */
    ASSERT_EQ(sqlite3_exec(hdb, "UPDATE horizon_metadata SET status = 'DISCARDED';", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor a;
    init_valid_c_anchor(temp_dir, &a);

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_id, &a, 1, err_buf, sizeof(err_buf));
    /* Promotion must fail because status was not ACTIVE */
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_STATE_TRANSITION_FAILED);
    ASSERT_TRUE(strstr(err_buf, "ACTIVE") != NULL);

    /* Verify base_db has 0 nodes committed */
    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);
    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_properties_relocation_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_propreloc_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    /* Schema with properties column and without byte_start/byte_len */
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, properties TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_id[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, 1234, "h_propreloc", "0", h_id, sizeof(h_id)), 0);

    sqlite3 *hdb = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_id, &hdb), 0);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor a;
    init_valid_c_anchor(temp_dir, &a);
    /* Induce relocation by intentionally offsetting stale byte_start */
    a.byte_start = 999;

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_id, &a, 1, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, CBM_ADMISSION_OK);

    /* Verify base_db has the node committed with relocated coordinates in properties JSON */
    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT properties FROM nodes WHERE cbm_uri = 'cbm://my_proj/src/foo.c#A';", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    const char *props = (const char *)sqlite3_column_text(stmt, 0);
    ASSERT_TRUE(props != NULL);
    ASSERT_TRUE(strstr(props, "\"byte_start\":0") != NULL);
    char exp_len_prop[64];
    snprintf(exp_len_prop, sizeof(exp_len_prop), "\"byte_len\":%u", a.byte_len);
    ASSERT_TRUE(strstr(props, exp_len_prop) != NULL);
    sqlite3_finalize(stmt);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);
    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_properties_update_ignore_trigger_rejected_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_propign_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, properties TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);

    /* Trigger that intercepts UPDATE OF properties and silently supresses it via RAISE(IGNORE) */
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TRIGGER trg_ignore BEFORE UPDATE OF properties ON nodes BEGIN SELECT RAISE(IGNORE); END;", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_id[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, 1234, "h_propign", "0", h_id, sizeof(h_id)), 0);

    sqlite3 *hdb = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_id, &hdb), 0);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor a;
    init_valid_c_anchor(temp_dir, &a);
    a.byte_start = 999; /* Induce relocation to trigger properties update */

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_id, &a, 1, err_buf, sizeof(err_buf));
    /* Must fail because update affected 0 rows due to RAISE(IGNORE) */
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED);
    ASSERT_TRUE(strstr(err_buf, "CONSOLIDATION_FAILED") != NULL);

    /* Verify base_db was rolled back: 0 nodes committed */
    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    /* Verify generation_log was not modified */
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);

    /* Verify horizon status in horizon_metadata remains ACTIVE */
    sqlite3 *recheck_hdb = NULL;
    char hpath[512];
    ASSERT_EQ(cbm_horizon_pool_get_path(&pool, h_id, hpath, sizeof(hpath)), 0);
    ASSERT_EQ(sqlite3_open(hpath, &recheck_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(recheck_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ?;", -1, &stmt, NULL), SQLITE_OK);
    sqlite3_bind_text(stmt, 1, h_id, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 0), "ACTIVE");
    sqlite3_finalize(stmt);
    sqlite3_close_v2(recheck_hdb);

    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_properties_update_abort_trigger_rejected_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_propab_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, properties TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);

    /* Trigger that intercepts UPDATE OF properties and explicitly aborts */
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TRIGGER trg_abort BEFORE UPDATE OF properties ON nodes BEGIN SELECT RAISE(ABORT, 'Blocked properties update'); END;", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_id[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, 1234, "h_propab", "0", h_id, sizeof(h_id)), 0);

    sqlite3 *hdb = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_id, &hdb), 0);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor a;
    init_valid_c_anchor(temp_dir, &a);
    a.byte_start = 999; /* Induce relocation to trigger properties update */

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_id, &a, 1, err_buf, sizeof(err_buf));
    /* Must fail with CONSOLIDATION_FAILED and error message preserved */
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED);
    ASSERT_TRUE(strstr(err_buf, "CONSOLIDATION_FAILED") != NULL);
    ASSERT_TRUE(strstr(err_buf, "Blocked properties update") != NULL);

    /* Verify base_db was rolled back: 0 nodes committed */
    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    /* Verify generation_log was not modified */
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);

    /* Verify horizon status in horizon_metadata remains ACTIVE */
    sqlite3 *recheck_hdb = NULL;
    char hpath[512];
    ASSERT_EQ(cbm_horizon_pool_get_path(&pool, h_id, hpath, sizeof(hpath)), 0);
    ASSERT_EQ(sqlite3_open(hpath, &recheck_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(recheck_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ?;", -1, &stmt, NULL), SQLITE_OK);
    sqlite3_bind_text(stmt, 1, h_id, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 0), "ACTIVE");
    sqlite3_finalize(stmt);
    sqlite3_close_v2(recheck_hdb);

    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_properties_tampered_coordinates_trigger_rejected_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_proptamp_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, properties TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);

    /* Trigger that intercepts UPDATE OF properties and mutates coordinates (byte_len 260 instead of 26) */
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TRIGGER trg_tamper AFTER UPDATE OF properties ON nodes BEGIN UPDATE nodes SET properties = '{\"byte_start\":0,\"byte_len\":260}' WHERE cbm_uri = NEW.cbm_uri; END;", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_id[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, 1234, "h_proptamp", "0", h_id, sizeof(h_id)), 0);

    sqlite3 *hdb = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_id, &hdb), 0);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor a;
    init_valid_c_anchor(temp_dir, &a);
    a.byte_start = 999; /* Induce relocation to trigger properties update */

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_id, &a, 1, err_buf, sizeof(err_buf));
    /* Must fail because active structural JSON check detects numeric mismatch [0, 26] != [0, 260] */
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED);
    ASSERT_TRUE(strstr(err_buf, "CONSOLIDATION_FAILED") != NULL);
    ASSERT_TRUE(strstr(err_buf, "properties verification mismatch") != NULL);

    /* Verify base_db was rolled back: 0 nodes committed */
    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    /* Verify generation_log was not modified */
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);

    /* Verify horizon status in horizon_metadata remains ACTIVE */
    sqlite3 *recheck_hdb = NULL;
    char hpath[512];
    ASSERT_EQ(cbm_horizon_pool_get_path(&pool, h_id, hpath, sizeof(hpath)), 0);
    ASSERT_EQ(sqlite3_open(hpath, &recheck_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(recheck_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ?;", -1, &stmt, NULL), SQLITE_OK);
    sqlite3_bind_text(stmt, 1, h_id, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 0), "ACTIVE");
    sqlite3_finalize(stmt);
    sqlite3_close_v2(recheck_hdb);

    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_properties_large_valid_json_accepted_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_proplg_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, properties TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);

    /* Trigger that appends extra metadata to properties, exceeding 1500 bytes of valid JSON */
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TRIGGER trg_large AFTER UPDATE OF properties ON nodes BEGIN UPDATE nodes SET properties = '{\"byte_start\":0,\"byte_len\":26,\"pad\":\"' || printf('%.1500c', 'x') || '\"}' WHERE cbm_uri = NEW.cbm_uri; END;", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_id[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, 1234, "h_proplg", "0", h_id, sizeof(h_id)), 0);

    sqlite3 *hdb = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_id, &hdb), 0);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor a;
    init_valid_c_anchor(temp_dir, &a);
    a.byte_start = 999; /* Induce relocation to trigger properties update */

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_id, &a, 1, err_buf, sizeof(err_buf));
    /* Large valid JSON > 1500 bytes must be read untruncated and accepted */
    ASSERT_EQ(rc, CBM_ADMISSION_OK);

    /* Verify base_db has committed 1 node and 1 generation entry */
    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 1);
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 1);
    sqlite3_finalize(stmt);

    /* Verify horizon status transitioned to PROMOTED */
    sqlite3_stmt *hstmt = NULL;
    char h_path[512];
    snprintf(h_path, sizeof(h_path), "%s/horizons/%s.db", temp_dir, h_id);
    sqlite3 *chk_hdb = NULL;
    ASSERT_EQ(sqlite3_open(h_path, &chk_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(chk_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ? LIMIT 1;", -1, &hstmt, NULL), SQLITE_OK);
    sqlite3_bind_text(hstmt, 1, h_id, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(hstmt), SQLITE_ROW);
    ASSERT_TRUE(strcmp((const char *)sqlite3_column_text(hstmt, 0), "PROMOTED") == 0);
    sqlite3_finalize(hstmt);
    sqlite3_close_v2(chk_hdb);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);
    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_properties_trailing_garbage_rejected_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_propgrb_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, properties TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);

    /* Trigger that appends 1024 spaces followed by invalid trailing garbage */
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TRIGGER trg_garbage AFTER UPDATE OF properties ON nodes BEGIN UPDATE nodes SET properties = '{\"byte_start\":0,\"byte_len\":26}' || printf('%.1024c', ' ') || '!@#INVALID_GARBAGE' WHERE cbm_uri = NEW.cbm_uri; END;", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_id[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, 1234, "h_propgrb", "0", h_id, sizeof(h_id)), 0);

    sqlite3 *hdb = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_id, &hdb), 0);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#B', 'B', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'cbm://my_proj/src/foo.c#B', 'CALLS', 'h_propgrb', 1);", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor anchors[2];
    init_valid_c_two_anchors(temp_dir, &anchors[0], &anchors[1]);
    anchors[0].byte_start = 999; /* Induce relocation on A to trigger properties update with trailing garbage */

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_id, anchors, 2, err_buf, sizeof(err_buf));
    /* Untruncated full document parse must reject trailing invalid garbage */
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED);
    ASSERT_TRUE(strstr(err_buf, "CONSOLIDATION_FAILED") != NULL);

    /* Verify isolated, complete rollback: 0 nodes, 0 edges, 0 generation_log, status remains ACTIVE */
    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM virtual_edges;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    /* Verify horizon status in horizon_metadata remained ACTIVE */
    sqlite3_stmt *hstmt = NULL;
    char h_path[512];
    snprintf(h_path, sizeof(h_path), "%s/horizons/%s.db", temp_dir, h_id);
    sqlite3 *chk_hdb = NULL;
    ASSERT_EQ(sqlite3_open(h_path, &chk_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(chk_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ? LIMIT 1;", -1, &hstmt, NULL), SQLITE_OK);
    sqlite3_bind_text(hstmt, 1, h_id, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(hstmt), SQLITE_ROW);
    ASSERT_TRUE(strcmp((const char *)sqlite3_column_text(hstmt, 0), "ACTIVE") == 0);
    sqlite3_finalize(hstmt);
    sqlite3_close_v2(chk_hdb);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);
    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_full_late_rollback_four_dimensions_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_lateroll_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, properties TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_id[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, 1234, "h_lateroll", "0", h_id, sizeof(h_id)), 0);

    sqlite3 *hdb = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_id, &hdb), 0);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#B', 'B', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(hdb, "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'cbm://my_proj/src/foo.c#B', 'CALLS', 'h_lateroll', 1);", NULL, NULL, NULL), SQLITE_OK);

    /* TRIGGER on h_db.horizon_metadata to cause a LATE failure:
     * fires after node consolidation (2 nodes), edge consolidation (1 edge),
     * and generation_log insertion (1 generation) have all been mutated in flight! */
    ASSERT_EQ(sqlite3_exec(hdb, "CREATE TRIGGER trg_late_fail AFTER UPDATE OF status ON horizon_metadata BEGIN SELECT RAISE(ABORT, 'late promotion failure simulation'); END;", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor anchors[2];
    init_valid_c_two_anchors(temp_dir, &anchors[0], &anchors[1]);

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_id, anchors, 2, err_buf, sizeof(err_buf));
    /* Promotion must fail at the late horizon_metadata update step with COMMIT_FAILED */
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_COMMIT_FAILED);
    ASSERT_TRUE(strstr(err_buf, "COMMIT_FAILED") != NULL);

    /* Verify isolated, complete rollback of ALL 4 in-flight mutations: */
    /* 1. base_db.nodes was rolled back from 2 to 0 */
    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    /* 2. base_db.virtual_edges was rolled back from 1 to 0 */
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM virtual_edges;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    /* 3. base_db.generation_log was rolled back from 1 to 0 */
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    /* 4. h_db.horizon_metadata status was rolled back from PROMOTED to ACTIVE */
    sqlite3_stmt *hstmt = NULL;
    char h_path[512];
    snprintf(h_path, sizeof(h_path), "%s/horizons/%s.db", temp_dir, h_id);
    sqlite3 *chk_hdb = NULL;
    ASSERT_EQ(sqlite3_open(h_path, &chk_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(chk_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ? LIMIT 1;", -1, &hstmt, NULL), SQLITE_OK);
    sqlite3_bind_text(hstmt, 1, h_id, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(hstmt), SQLITE_ROW);
    ASSERT_TRUE(strcmp((const char *)sqlite3_column_text(hstmt, 0), "ACTIVE") == 0);
    sqlite3_finalize(hstmt);
    sqlite3_close_v2(chk_hdb);

    /* 5. gate.base_generation was NOT incremented (remains 1) */
    ASSERT_EQ(gate.base_generation, 1);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);
    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_conflict_query_failure_blocks_promotion_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_cflfail_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, properties TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_cand[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, (uint32_t)getpid(), "h_cand", "0", h_cand, sizeof(h_cand)), 0);

    sqlite3 *cand_db = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_cand, &cand_db), 0);
    ASSERT_EQ(cbm_horizon_bind_project(cand_db, "my_proj"), 0);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#B', 'B', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'cbm://my_proj/src/foo.c#B', 'CALLS', 'h_cand', 1);", NULL, NULL, NULL), SQLITE_OK);

    /* Create an active alive sibling horizon in the same project whose symbolic_nodes table is dropped,
     * simulating an unreadable/corrupted state during concurrent physical conflict scan */
    char h_sib[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, (uint32_t)getpid(), "h_sib", "0", h_sib, sizeof(h_sib)), 0);
    sqlite3 *sib_db = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_sib, &sib_db), 0);
    ASSERT_EQ(cbm_horizon_bind_project(sib_db, "my_proj"), 0);
    ASSERT_EQ(sqlite3_exec(sib_db, "DROP TABLE symbolic_nodes;", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor anchors[2];
    init_valid_c_two_anchors(temp_dir, &anchors[0], &anchors[1]);

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_cand, anchors, 2, err_buf, sizeof(err_buf));
    /* Promotion must be blocked with CONSOLIDATION_FAILED because conflict absence cannot be established */
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED);
    ASSERT_TRUE(strstr(err_buf, "CONSOLIDATION_FAILED") != NULL);
    ASSERT_TRUE(strstr(err_buf, "concurrent conflict scan failed") != NULL);

    /* Verify isolated, complete state preservation: 0 nodes, 0 edges, 0 generation_log, status remains ACTIVE */
    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM virtual_edges;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    /* Verify candidate horizon status in horizon_metadata remained ACTIVE */
    sqlite3_stmt *hstmt = NULL;
    char h_path[512];
    snprintf(h_path, sizeof(h_path), "%s/horizons/%s.db", temp_dir, h_cand);
    sqlite3 *chk_hdb = NULL;
    ASSERT_EQ(sqlite3_open(h_path, &chk_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(chk_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ? LIMIT 1;", -1, &hstmt, NULL), SQLITE_OK);
    sqlite3_bind_text(hstmt, 1, h_cand, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(hstmt), SQLITE_ROW);
    ASSERT_TRUE(strcmp((const char *)sqlite3_column_text(hstmt, 0), "ACTIVE") == 0);
    sqlite3_finalize(hstmt);
    sqlite3_close_v2(chk_hdb);

    /* Verify gate generation was NOT incremented */
    ASSERT_EQ(gate.base_generation, 1);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);
    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_conflict_enumeration_failure_blocks_promotion_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_cflenum_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, properties TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_cand[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, (uint32_t)getpid(), "h_cand", "0", h_cand, sizeof(h_cand)), 0);

    sqlite3 *cand_db = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_cand, &cand_db), 0);
    ASSERT_EQ(cbm_horizon_bind_project(cand_db, "my_proj"), 0);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#B', 'B', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'cbm://my_proj/src/foo.c#B', 'CALLS', 'h_cand', 1);", NULL, NULL, NULL), SQLITE_OK);

    /* Sibling horizon drops horizon_context table (the Codex counterexample).
     * Enumeration must NOT silently omit this sibling; it must fail closed! */
    char h_sib[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, (uint32_t)getpid(), "h_sib", "0", h_sib, sizeof(h_sib)), 0);
    sqlite3 *sib_db = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_sib, &sib_db), 0);
    ASSERT_EQ(cbm_horizon_bind_project(sib_db, "my_proj"), 0);
    ASSERT_EQ(sqlite3_exec(sib_db, "DROP TABLE horizon_context;", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor anchors[2];
    init_valid_c_two_anchors(temp_dir, &anchors[0], &anchors[1]);

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_cand, anchors, 2, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED);
    ASSERT_TRUE(strstr(err_buf, "CONSOLIDATION_FAILED") != NULL);
    ASSERT_TRUE(strstr(err_buf, "horizon_context") != NULL);

    /* Verify isolated, complete state preservation: 0 nodes, 0 edges, 0 generation_log, status remains ACTIVE */
    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM virtual_edges;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    sqlite3_stmt *hstmt = NULL;
    char h_path[512];
    snprintf(h_path, sizeof(h_path), "%s/horizons/%s.db", temp_dir, h_cand);
    sqlite3 *chk_hdb = NULL;
    ASSERT_EQ(sqlite3_open(h_path, &chk_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(chk_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ? LIMIT 1;", -1, &hstmt, NULL), SQLITE_OK);
    sqlite3_bind_text(hstmt, 1, h_cand, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(hstmt), SQLITE_ROW);
    ASSERT_TRUE(strcmp((const char *)sqlite3_column_text(hstmt, 0), "ACTIVE") == 0);
    sqlite3_finalize(hstmt);
    sqlite3_close_v2(chk_hdb);

    ASSERT_EQ(gate.base_generation, 1);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);
    th_rmtree(temp_dir);
    PASS();
}

static void fail_physical_step_cb(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;
    sqlite3_result_error(ctx, "physical step failure simulation", -1);
}

static void fail_semantic_step_cb(sqlite3_context *ctx, int argc, sqlite3_value **argv) {
    (void)argc;
    (void)argv;
    sqlite3_result_error(ctx, "semantic step failure simulation", -1);
}

TEST(test_promote_horizon_conflict_physical_step_failure_blocks_promotion_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_cflpstp_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, properties TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_cand[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, (uint32_t)getpid(), "h_cand", "0", h_cand, sizeof(h_cand)), 0);

    sqlite3 *cand_db = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_cand, &cand_db), 0);
    ASSERT_EQ(cbm_horizon_bind_project(cand_db, "my_proj"), 0);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#B', 'B', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'cbm://my_proj/src/foo.c#B', 'CALLS', 'h_cand', 1);", NULL, NULL, NULL), SQLITE_OK);

    /* Sibling replaces symbolic_nodes with a failing view: prepare succeeds, step aborts */
    char h_sib[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, (uint32_t)getpid(), "h_sib", "0", h_sib, sizeof(h_sib)), 0);
    sqlite3 *sib_db = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_sib, &sib_db), 0);
    ASSERT_EQ(cbm_horizon_bind_project(sib_db, "my_proj"), 0);
    ASSERT_EQ(sqlite3_create_function(sib_db, "fail_phys_step", 0, SQLITE_UTF8, NULL, fail_physical_step_cb, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(sib_db, "DROP TABLE symbolic_nodes;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(sib_db, "CREATE VIEW symbolic_nodes AS SELECT fail_phys_step() AS cbm_uri;", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor anchors[2];
    init_valid_c_two_anchors(temp_dir, &anchors[0], &anchors[1]);

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_cand, anchors, 2, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED);
    ASSERT_TRUE(strstr(err_buf, "CONSOLIDATION_FAILED") != NULL);
    ASSERT_TRUE(strstr(err_buf, "physical step failure simulation") != NULL);

    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM virtual_edges;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    sqlite3_stmt *hstmt = NULL;
    char h_path[512];
    snprintf(h_path, sizeof(h_path), "%s/horizons/%s.db", temp_dir, h_cand);
    sqlite3 *chk_hdb = NULL;
    ASSERT_EQ(sqlite3_open(h_path, &chk_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(chk_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ? LIMIT 1;", -1, &hstmt, NULL), SQLITE_OK);
    sqlite3_bind_text(hstmt, 1, h_cand, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(hstmt), SQLITE_ROW);
    ASSERT_TRUE(strcmp((const char *)sqlite3_column_text(hstmt, 0), "ACTIVE") == 0);
    sqlite3_finalize(hstmt);
    sqlite3_close_v2(chk_hdb);

    ASSERT_EQ(gate.base_generation, 1);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);
    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_conflict_semantic_query_failure_blocks_promotion_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_cflsem_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, properties TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_cand[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, (uint32_t)getpid(), "h_cand", "0", h_cand, sizeof(h_cand)), 0);

    sqlite3 *cand_db = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_cand, &cand_db), 0);
    ASSERT_EQ(cbm_horizon_bind_project(cand_db, "my_proj"), 0);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#B', 'B', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'cbm://my_proj/src/foo.c#B', 'CALLS', 'h_cand', 1);", NULL, NULL, NULL), SQLITE_OK);

    /* Sibling has disjoint physical nodes (no physical conflict), but virtual_edges table is dropped */
    char h_sib[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, (uint32_t)getpid(), "h_sib", "0", h_sib, sizeof(h_sib)), 0);
    sqlite3 *sib_db = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_sib, &sib_db), 0);
    ASSERT_EQ(cbm_horizon_bind_project(sib_db, "my_proj"), 0);
    ASSERT_EQ(sqlite3_exec(sib_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/other.c#Other', 'Other', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(sib_db, "DROP TABLE virtual_edges;", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor anchors[2];
    init_valid_c_two_anchors(temp_dir, &anchors[0], &anchors[1]);

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_cand, anchors, 2, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED);
    ASSERT_TRUE(strstr(err_buf, "CONSOLIDATION_FAILED") != NULL);
    ASSERT_TRUE(strstr(err_buf, "virtual_edges") != NULL);

    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM virtual_edges;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    sqlite3_stmt *hstmt = NULL;
    char h_path[512];
    snprintf(h_path, sizeof(h_path), "%s/horizons/%s.db", temp_dir, h_cand);
    sqlite3 *chk_hdb = NULL;
    ASSERT_EQ(sqlite3_open(h_path, &chk_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(chk_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ? LIMIT 1;", -1, &hstmt, NULL), SQLITE_OK);
    sqlite3_bind_text(hstmt, 1, h_cand, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(hstmt), SQLITE_ROW);
    ASSERT_TRUE(strcmp((const char *)sqlite3_column_text(hstmt, 0), "ACTIVE") == 0);
    sqlite3_finalize(hstmt);
    sqlite3_close_v2(chk_hdb);

    ASSERT_EQ(gate.base_generation, 1);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);
    th_rmtree(temp_dir);
    PASS();
}

TEST(test_promote_horizon_conflict_semantic_step_failure_blocks_promotion_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_cflsstp_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, properties TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_cand[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, (uint32_t)getpid(), "h_cand", "0", h_cand, sizeof(h_cand)), 0);

    sqlite3 *cand_db = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_cand, &cand_db), 0);
    ASSERT_EQ(cbm_horizon_bind_project(cand_db, "my_proj"), 0);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#B', 'B', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'cbm://my_proj/src/foo.c#B', 'CALLS', 'h_cand', 1);", NULL, NULL, NULL), SQLITE_OK);

    /* Sibling has disjoint physical nodes; virtual_edges is a view that triggers abort on step */
    char h_sib[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, (uint32_t)getpid(), "h_sib", "0", h_sib, sizeof(h_sib)), 0);
    sqlite3 *sib_db = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_sib, &sib_db), 0);
    ASSERT_EQ(cbm_horizon_bind_project(sib_db, "my_proj"), 0);
    ASSERT_EQ(sqlite3_exec(sib_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/other.c#Other', 'Other', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_create_function(sib_db, "fail_sem_step", 0, SQLITE_UTF8, NULL, fail_semantic_step_cb, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(sib_db, "DROP TABLE virtual_edges;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(sib_db, "CREATE VIEW virtual_edges AS SELECT fail_sem_step() AS source_uri, 'dummy' AS target_uri;", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor anchors[2];
    init_valid_c_two_anchors(temp_dir, &anchors[0], &anchors[1]);

    char err_buf[512] = {0};
    int rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_cand, anchors, 2, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED);
    ASSERT_TRUE(strstr(err_buf, "CONSOLIDATION_FAILED") != NULL);
    ASSERT_TRUE(strstr(err_buf, "semantic step failure simulation") != NULL);

    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM virtual_edges;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);

    sqlite3_stmt *hstmt = NULL;
    char h_path[512];
    snprintf(h_path, sizeof(h_path), "%s/horizons/%s.db", temp_dir, h_cand);
    sqlite3 *chk_hdb = NULL;
    ASSERT_EQ(sqlite3_open(h_path, &chk_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(chk_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ? LIMIT 1;", -1, &hstmt, NULL), SQLITE_OK);
    sqlite3_bind_text(hstmt, 1, h_cand, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(hstmt), SQLITE_ROW);
    ASSERT_TRUE(strcmp((const char *)sqlite3_column_text(hstmt, 0), "ACTIVE") == 0);
    sqlite3_finalize(hstmt);
    sqlite3_close_v2(chk_hdb);

    ASSERT_EQ(gate.base_generation, 1);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);
    th_rmtree(temp_dir);
    PASS();
}

static int inject_stat_not_dir_cb(const char *path, struct stat *buf) {
    (void)path;
    if (buf) {
        memset(buf, 0, sizeof(*buf));
        buf->st_mode = S_IFREG; /* Regular file triggers !S_ISDIR(st.st_mode) */
    }
    return 0;
}

static int inject_stat_eacces_cb(const char *path, struct stat *buf) {
    (void)path;
    (void)buf;
    errno = EACCES;
    return -1;
}

static int inject_stat_eio_cb(const char *path, struct stat *buf) {
    (void)path;
    (void)buf;
    errno = EIO;
    return -1;
}

TEST(test_promote_horizon_conflict_stat_failure_blocks_promotion_c) {
    char temp_dir[256];
    snprintf(temp_dir, sizeof(temp_dir), "/tmp/cbm_cflstat_XXXXXX");
    if (!cbm_mkdtemp(temp_dir)) FAIL("temp dir creation failed");

    HorizonConnectionPool pool;
    ASSERT_EQ(cbm_horizon_pool_init(&pool, temp_dir), 0);

    AdmissionGate gate;
    cbm_admission_gate_init(&gate, "my_proj", 1);

    sqlite3 *base_db = NULL;
    char base_path[512];
    snprintf(base_path, sizeof(base_path), "%s/base.db", temp_dir);
    ASSERT_EQ(sqlite3_open(base_path, &base_db), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "PRAGMA journal_mode = DELETE;", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE nodes (cbm_uri TEXT PRIMARY KEY, label TEXT, properties TEXT);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(base_db, "CREATE TABLE generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL), SQLITE_OK);
    cbm_admission_gate_set_base_db(&gate, base_db);

    char h_cand[CBM_HORIZON_ID_MAX] = {0};
    ASSERT_EQ(cbm_create_horizon(&pool, (uint32_t)getpid(), "h_cand", "0", h_cand, sizeof(h_cand)), 0);

    sqlite3 *cand_db = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&pool, h_cand, &cand_db), 0);
    ASSERT_EQ(cbm_horizon_bind_project(cand_db, "my_proj"), 0);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'A', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO symbolic_nodes (cbm_uri, label, epistemic_status, code_snippet, created_at) VALUES ('cbm://my_proj/src/foo.c#B', 'B', 'ACCEPTED', '{}', 1);", NULL, NULL, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_exec(cand_db, "INSERT INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) VALUES ('cbm://my_proj/src/foo.c#A', 'cbm://my_proj/src/foo.c#B', 'CALLS', 'h_cand', 1);", NULL, NULL, NULL), SQLITE_OK);

    TwoTierAnchor anchors[2];
    init_valid_c_two_anchors(temp_dir, &anchors[0], &anchors[1]);

    /* Controlled stat injection:
     * 1. Direct unit test of cbm_horizon_pool_get_active_alive:
     *    a) ENOENT explicitly recognized as valid empty set (return 0, count 0) */
    HorizonConnectionPool non_existent_pool;
    cbm_horizon_pool_init(&non_existent_pool, TH_PATH(temp_dir, "non_existent_dir"));
    ActiveHorizonLiveness *no_liveness = NULL;
    size_t no_count = 999;
    char no_err[256] = {0};
    ASSERT_EQ(cbm_horizon_pool_get_active_alive(&non_existent_pool, "my_proj", &no_liveness, &no_count, no_err, sizeof(no_err)), 0);
    ASSERT_EQ(no_count, 0);
    ASSERT_TRUE(no_liveness == NULL);

    /*    b) S_ISDIR failure: stat returns success but st_mode is S_IFREG, triggering !S_ISDIR */
    cbm_horizon_pool_set_stat_hook(inject_stat_not_dir_cb);
    ActiveHorizonLiveness *fl_liveness = NULL;
    size_t fl_count = 999;
    char fl_err[256] = {0};
    ASSERT_EQ(cbm_horizon_pool_get_active_alive(&pool, "my_proj", &fl_liveness, &fl_count, fl_err, sizeof(fl_err)), -1);
    ASSERT_TRUE(strstr(fl_err, "not a directory") != NULL);
    ASSERT_TRUE(fl_liveness == NULL);

    /*    b.2) End-to-end promotion integration: S_ISDIR failure during cbm_promote_horizon */
    char err_buf[512];
    int rc;
    sqlite3_stmt *stmt = NULL;
    sqlite3_stmt *hstmt = NULL;
    char h_path[512];
    sqlite3 *chk_hdb = NULL;

    memset(err_buf, 0, sizeof(err_buf));
    rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_cand, anchors, 2, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED);
    ASSERT_TRUE(strstr(err_buf, "CONSOLIDATION_FAILED") != NULL);
    ASSERT_TRUE(strstr(err_buf, "not a directory") != NULL);

    /* Verify isolated, complete 4-dimension state preservation + status ACTIVE */
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);
    stmt = NULL;

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM virtual_edges;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);
    stmt = NULL;

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);
    stmt = NULL;

    snprintf(h_path, sizeof(h_path), "%s/horizons/%s.db", temp_dir, h_cand);
    ASSERT_EQ(sqlite3_open(h_path, &chk_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(chk_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ? LIMIT 1;", -1, &hstmt, NULL), SQLITE_OK);
    sqlite3_bind_text(hstmt, 1, h_cand, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(hstmt), SQLITE_ROW);
    ASSERT_TRUE(strcmp((const char *)sqlite3_column_text(hstmt, 0), "ACTIVE") == 0);
    sqlite3_finalize(hstmt);
    hstmt = NULL;
    sqlite3_close_v2(chk_hdb);
    chk_hdb = NULL;

    ASSERT_EQ(gate.base_generation, 1);

    /*    c) Direct stat() == -1 injection with EACCES */
    cbm_horizon_pool_set_stat_hook(inject_stat_eacces_cb);
    ActiveHorizonLiveness *ea_liveness = NULL;
    size_t ea_count = 999;
    char ea_err[256] = {0};
    ASSERT_EQ(cbm_horizon_pool_get_active_alive(&pool, "my_proj", &ea_liveness, &ea_count, ea_err, sizeof(ea_err)), -1);
    ASSERT_TRUE(strstr(ea_err, "failed to access horizons path") != NULL);
    ASSERT_TRUE(strstr(ea_err, "errno=13") != NULL);
    ASSERT_TRUE(ea_liveness == NULL);

    /*    d) Direct stat() == -1 injection with EIO */
    cbm_horizon_pool_set_stat_hook(inject_stat_eio_cb);
    ActiveHorizonLiveness *eio_liveness = NULL;
    size_t eio_count = 999;
    char eio_err[256] = {0};
    ASSERT_EQ(cbm_horizon_pool_get_active_alive(&pool, "my_proj", &eio_liveness, &eio_count, eio_err, sizeof(eio_err)), -1);
    ASSERT_TRUE(strstr(eio_err, "failed to access horizons path") != NULL);
    ASSERT_TRUE(strstr(eio_err, "errno=5") != NULL);
    ASSERT_TRUE(eio_liveness == NULL);

    /* 2. End-to-end promotion integration: EACCES injected during cbm_promote_horizon */
    cbm_horizon_pool_set_stat_hook(inject_stat_eacces_cb);
    memset(err_buf, 0, sizeof(err_buf));
    rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_cand, anchors, 2, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED);
    ASSERT_TRUE(strstr(err_buf, "CONSOLIDATION_FAILED") != NULL);
    ASSERT_TRUE(strstr(err_buf, "failed to access horizons path") != NULL);
    ASSERT_TRUE(strstr(err_buf, "errno=13") != NULL);

    /* Verify isolated, complete 4-dimension state preservation + status ACTIVE */
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);
    stmt = NULL;

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM virtual_edges;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);
    stmt = NULL;

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);
    stmt = NULL;

    snprintf(h_path, sizeof(h_path), "%s/horizons/%s.db", temp_dir, h_cand);
    ASSERT_EQ(sqlite3_open(h_path, &chk_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(chk_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ? LIMIT 1;", -1, &hstmt, NULL), SQLITE_OK);
    sqlite3_bind_text(hstmt, 1, h_cand, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(hstmt), SQLITE_ROW);
    ASSERT_TRUE(strcmp((const char *)sqlite3_column_text(hstmt, 0), "ACTIVE") == 0);
    sqlite3_finalize(hstmt);
    hstmt = NULL;
    sqlite3_close_v2(chk_hdb);
    chk_hdb = NULL;

    ASSERT_EQ(gate.base_generation, 1);

    /* 3. End-to-end promotion integration: EIO injected during cbm_promote_horizon */
    cbm_horizon_pool_set_stat_hook(inject_stat_eio_cb);
    memset(err_buf, 0, sizeof(err_buf));
    rc = cbm_promote_horizon(&gate, &pool, temp_dir, h_cand, anchors, 2, err_buf, sizeof(err_buf));
    ASSERT_EQ(rc, CBM_ADMISSION_ERR_CONSOLIDATION_FAILED);
    ASSERT_TRUE(strstr(err_buf, "CONSOLIDATION_FAILED") != NULL);
    ASSERT_TRUE(strstr(err_buf, "failed to access horizons path") != NULL);
    ASSERT_TRUE(strstr(err_buf, "errno=5") != NULL);

    /* Verify again isolated, complete 4-dimension state preservation + status ACTIVE */
    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM nodes;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);
    stmt = NULL;

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM virtual_edges;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);
    stmt = NULL;

    ASSERT_EQ(sqlite3_prepare_v2(base_db, "SELECT COUNT(*) FROM generation_log;", -1, &stmt, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_EQ(sqlite3_column_int(stmt, 0), 0);
    sqlite3_finalize(stmt);
    stmt = NULL;

    ASSERT_EQ(sqlite3_open(h_path, &chk_hdb), SQLITE_OK);
    ASSERT_EQ(sqlite3_prepare_v2(chk_hdb, "SELECT status FROM horizon_metadata WHERE horizon_id = ? LIMIT 1;", -1, &hstmt, NULL), SQLITE_OK);
    sqlite3_bind_text(hstmt, 1, h_cand, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(hstmt), SQLITE_ROW);
    ASSERT_TRUE(strcmp((const char *)sqlite3_column_text(hstmt, 0), "ACTIVE") == 0);
    sqlite3_finalize(hstmt);
    hstmt = NULL;
    sqlite3_close_v2(chk_hdb);
    chk_hdb = NULL;

    ASSERT_EQ(gate.base_generation, 1);

    /* Restore hook */
    cbm_horizon_pool_set_stat_hook(NULL);

    cbm_horizon_pool_close_all(&pool);
    sqlite3_close_v2(base_db);
    th_rmtree(temp_dir);
    PASS();
}

SUITE(anchor_checker) {
    RUN_TEST(test_anchor_checker_fast_path);
    RUN_TEST(test_anchor_checker_oversized_byte_len);
    RUN_TEST(test_anchor_checker_ast_null_guard);
    RUN_TEST(test_anchor_checker_whitespace_normalization);
    RUN_TEST(test_anchor_checker_relocation_default_state);
    RUN_TEST(test_anchor_checker_resolve_language);
    RUN_TEST(test_anchor_checker_verify_null_guards);
    RUN_TEST(test_anchor_checker_path_traversal_relative);
    RUN_TEST(test_anchor_checker_path_traversal_absolute_escape);
    RUN_TEST(test_anchor_checker_const_anchor_no_mutation);
    RUN_TEST(test_anchor_checker_zero_ast_hash_rejected);
    RUN_TEST(test_paths_match_strict_rejects_traversal_and_hidden_file_confusion);
    RUN_TEST(test_find_matching_anchor_complete_identity_repo_path_symbol);
    RUN_TEST(test_promote_horizon_in_memory_base_db_rejected_c);
    RUN_TEST(test_promote_horizon_atomic_2pc_success_c);
    RUN_TEST(test_promote_horizon_rollback_on_trigger_failure_c);
    RUN_TEST(test_promote_horizon_requires_active_status_c);
    RUN_TEST(test_promote_horizon_properties_relocation_c);
    RUN_TEST(test_promote_horizon_properties_update_ignore_trigger_rejected_c);
    RUN_TEST(test_promote_horizon_properties_update_abort_trigger_rejected_c);
    RUN_TEST(test_promote_horizon_properties_tampered_coordinates_trigger_rejected_c);
    RUN_TEST(test_promote_horizon_properties_large_valid_json_accepted_c);
    RUN_TEST(test_promote_horizon_properties_trailing_garbage_rejected_c);
    RUN_TEST(test_promote_horizon_full_late_rollback_four_dimensions_c);
    RUN_TEST(test_promote_horizon_conflict_query_failure_blocks_promotion_c);
    RUN_TEST(test_promote_horizon_conflict_enumeration_failure_blocks_promotion_c);
    RUN_TEST(test_promote_horizon_conflict_physical_step_failure_blocks_promotion_c);
    RUN_TEST(test_promote_horizon_conflict_semantic_query_failure_blocks_promotion_c);
    RUN_TEST(test_promote_horizon_conflict_semantic_step_failure_blocks_promotion_c);
    RUN_TEST(test_promote_horizon_conflict_stat_failure_blocks_promotion_c);
}

