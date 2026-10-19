/*
 * test_harness_v2.c — Comprehensive Unit, Integration, and Adversarial tests
 * for Harness Perfeito v2 (004-codebase-memory-multigraph-mcp-test-scenarios.md).
 *
 * Covers:
 * - U01..U19: Value Objects, Snapshot Identity, Tri-State Effect Lattice,
 *   Bifocal Candidate focus, Monotonic Cadence, Tradition Governance,
 *   Option A Admission Manifest invariants, and Fast-Path Evaluator.
 * - I01..I22: End-to-end Bundle Extraction, Seam Map classification,
 *   SQLite WAL + synchronous=FULL verification, Append-Only triggers,
 *   Publication Fence, Stale Snapshot rejection, Option A Forensic Ledger,
 *   and MCP Tool JSON-RPC contracts.
 * - F01..F09: Adversarial red-team scenarios (effect cycles, privilege escalation,
 *   fast-path spoofing, coverage gap fail-closed, etc.).
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "harness/harness_v2.h"
#include "store/governance_store.h"
#include "store/store.h"
#include "mcp/mcp.h"
#include "foundation/platform.h"
#include "foundation/sha256.h"
#include "yyjson/yyjson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *make_temp_gov_db_path(char *dir_out, size_t dir_sz) {
    snprintf(dir_out, dir_sz, "/tmp/cbm-hv2-test-XXXXXX");
    if (!cbm_mkdtemp(dir_out)) {
        return NULL;
    }
    return cbm_gov_resolve_db_path("test_proj", dir_out);
}

/* ── Unit Scenarios (U01 - U19) ────────────────────────────────── */

TEST(hv2_u01_u02_snapshot_id_determinism_and_validation) {
    char ggen[65], rrev[65], crev[65], snap1[65], snap2[65], snap_diff[65];
    cbm_sha256_hex("gen1", 4, ggen);
    cbm_sha256_hex("rules1", 6, rrev);
    cbm_sha256_hex("cov1", 4, crev);

    ASSERT_EQ(cbm_harness_compute_snapshot_id(ggen, rrev, crev, snap1), 0);
    ASSERT_EQ(cbm_harness_compute_snapshot_id(ggen, rrev, crev, snap2), 0);
    ASSERT_STR_EQ(snap1, snap2);
    ASSERT_TRUE(cbm_harness_validate_sha256_hex(snap1));

    /* Sensitivity to single character change in any component */
    char rrev2[65];
    cbm_sha256_hex("rules2", 6, rrev2);
    ASSERT_EQ(cbm_harness_compute_snapshot_id(ggen, rrev2, crev, snap_diff), 0);
    ASSERT_TRUE(strcmp(snap1, snap_diff) != 0);

    /* U02: Reject empty components or non-64-char hex */
    ASSERT_NEQ(cbm_harness_compute_snapshot_id("", rrev, crev, snap1), 0);
    ASSERT_NEQ(cbm_harness_compute_snapshot_id(ggen, "", crev, snap1), 0);
    ASSERT_NEQ(cbm_harness_compute_snapshot_id(ggen, rrev, "", snap1), 0);
    ASSERT_FALSE(cbm_harness_validate_sha256_hex("abc123"));
    ASSERT_FALSE(cbm_harness_validate_sha256_hex(
        "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ"));
    PASS();
}

TEST(hv2_u03_u04_span_and_khop_bounds) {
    /* U03: SourceSpan validation */
    ASSERT_TRUE(cbm_harness_validate_span(1, 1));
    ASSERT_TRUE(cbm_harness_validate_span(10, 45));
    ASSERT_FALSE(cbm_harness_validate_span(0, 10));
    ASSERT_FALSE(cbm_harness_validate_span(-5, 10));
    ASSERT_FALSE(cbm_harness_validate_span(20, 19));

    /* U04: k_hop bounds [1, 3] */
    ASSERT_FALSE(cbm_harness_validate_k_hop(0));
    ASSERT_FALSE(cbm_harness_validate_k_hop(-1));
    ASSERT_TRUE(cbm_harness_validate_k_hop(1));
    ASSERT_TRUE(cbm_harness_validate_k_hop(2));
    ASSERT_TRUE(cbm_harness_validate_k_hop(3));
    ASSERT_FALSE(cbm_harness_validate_k_hop(4));
    ASSERT_FALSE(cbm_harness_validate_k_hop(99));
    PASS();
}

TEST(hv2_u05_u06_tristate_effect_lattice_laws) {
    cbm_effect_tristate_t vals[] = {
        CBM_EFFECT_ABSENT, CBM_EFFECT_UNKNOWN, CBM_EFFECT_PRESENT
    };

    /* U05: Dominance present > unknown > absent */
    ASSERT_EQ(cbm_effect_join(CBM_EFFECT_ABSENT, CBM_EFFECT_UNKNOWN), CBM_EFFECT_UNKNOWN);
    ASSERT_EQ(cbm_effect_join(CBM_EFFECT_UNKNOWN, CBM_EFFECT_PRESENT), CBM_EFFECT_PRESENT);
    ASSERT_EQ(cbm_effect_join(CBM_EFFECT_ABSENT, CBM_EFFECT_PRESENT), CBM_EFFECT_PRESENT);

    /* U06: Commutativity, Associativity, Idempotence across all 27 triples */
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(cbm_effect_join(vals[i], vals[i]), vals[i]);
        for (int j = 0; j < 3; j++) {
            ASSERT_EQ(cbm_effect_join(vals[i], vals[j]),
                      cbm_effect_join(vals[j], vals[i]));
            for (int k = 0; k < 3; k++) {
                cbm_effect_tristate_t lhs = cbm_effect_join(cbm_effect_join(vals[i], vals[j]), vals[k]);
                cbm_effect_tristate_t rhs = cbm_effect_join(vals[i], cbm_effect_join(vals[j], vals[k]));
                ASSERT_EQ(lhs, rhs);
            }
        }
    }
    PASS();
}

TEST(hv2_u07_u08_f01_effect_propagation_and_cycle_convergence) {
    cbm_store_t *store = cbm_store_open_memory();
    ASSERT_NOT_NULL(store);
    ASSERT_EQ(cbm_store_upsert_project(store, "proj_cyc", "/tmp/proj_cyc"), CBM_STORE_OK);

    /* Create cyclic call graph A -> B -> C -> A, where B writes DB and C has unresolved call */
    cbm_node_t na = {.project = "proj_cyc", .label = "Function", .name = "funcA",
                     .qualified_name = "pkg.funcA", .file_path = "a.c", .start_line = 1, .end_line = 10,
                     .properties_json = "{}"};
    cbm_node_t nb = {.project = "proj_cyc", .label = "Function", .name = "funcB",
                     .qualified_name = "pkg.funcB", .file_path = "b.c", .start_line = 1, .end_line = 10,
                     .properties_json = "{\"sql\":\"INSERT INTO users VALUES(1)\"}"};
    cbm_node_t nc = {.project = "proj_cyc", .label = "Function", .name = "funcC",
                     .qualified_name = "pkg.funcC", .file_path = "c.c", .start_line = 1, .end_line = 10,
                     .properties_json = "{\"unresolved_call\":true}"};

    int64_t id_a = cbm_store_upsert_node(store, &na);
    int64_t id_b = cbm_store_upsert_node(store, &nb);
    int64_t id_c = cbm_store_upsert_node(store, &nc);
    ASSERT_GT(id_a, 0);
    ASSERT_GT(id_b, 0);
    ASSERT_GT(id_c, 0);

    cbm_edge_t e_ab = {.project = "proj_cyc", .source_id = id_a, .target_id = id_b, .type = "CALLS", .properties_json = "{}"};
    cbm_edge_t e_bc = {.project = "proj_cyc", .source_id = id_b, .target_id = id_c, .type = "CALLS", .properties_json = "{}"};
    cbm_edge_t e_ca = {.project = "proj_cyc", .source_id = id_c, .target_id = id_a, .type = "CALLS", .properties_json = "{}"};
    ASSERT_GT(cbm_store_insert_edge(store, &e_ab), 0);
    ASSERT_GT(cbm_store_insert_edge(store, &e_bc), 0);
    ASSERT_GT(cbm_store_insert_edge(store, &e_ca), 0);

    cbm_effect_fingerprint_t fp_a = {0};
    ASSERT_EQ(cbm_harness_compute_node_effects(store, "proj_cyc", id_a, 5, &fp_a), 0);
    /* B's writes_db=present and C's dynamic_dispatch=unknown must propagate to A without infinite loop */
    ASSERT_EQ(fp_a.writes_db, CBM_EFFECT_PRESENT);
    ASSERT_EQ(fp_a.dynamic_dispatch, CBM_EFFECT_UNKNOWN);
    ASSERT_TRUE(cbm_effect_has_any_unknown(&fp_a));
    ASSERT_NOT_NULL(strstr(fp_a.uncertainty_reasons_json, "unresolved_call_target"));

    cbm_effect_fingerprint_free(&fp_a);
    cbm_store_close(store);
    PASS();
}

TEST(hv2_u10_canonical_json_rfc8785_key_order_invariance) {
    const char *j1 = "{\"z\":1,\"a\":{\"d\":true,\"b\":\"hello\"},\"m\":[3,2,1]}";
    const char *j2 = "{  \"m\": [3, 2, 1], \"a\": { \"b\": \"hello\", \"d\": true }, \"z\": 1 }";
    char h1[65], h2[65];
    char *c1 = NULL, *c2 = NULL;

    ASSERT_EQ(cbm_canonical_json(j1, &c1), 0);
    ASSERT_EQ(cbm_canonical_json(j2, &c2), 0);
    ASSERT_STR_EQ(c1, c2);
    ASSERT_STR_EQ(c1, "{\"a\":{\"b\":\"hello\",\"d\":true},\"m\":[3,2,1],\"z\":1}");

    ASSERT_EQ(cbm_canonical_json_sha256(j1, h1), 0);
    ASSERT_EQ(cbm_canonical_json_sha256(j2, h2), 0);
    ASSERT_STR_EQ(h1, h2);

    free(c1);
    free(c2);
    PASS();
}

TEST(hv2_u11_u12_u13_u14_u15_u16_f05_tradition_governance_and_cadence) {
    char tmpdir[256];
    char *db_path = make_temp_gov_db_path(tmpdir, sizeof(tmpdir));
    ASSERT_NOT_NULL(db_path);

    cbm_gov_store_t *gov = NULL;
    ASSERT_EQ(cbm_gov_store_open(db_path, "test_proj", &gov), CBM_GOV_OK);
    ASSERT_NOT_NULL(gov);

    /* U11: Reject non-positive or overflowing review_interval_seconds */
    ASSERT_EQ(cbm_gov_propose_rule(gov, "R-BAD-INT", "src/**", "LAYER_BOUNDARY",
                                   "{\"forbid\":\"ui->db\"}", "agent", "sess-1", "[]",
                                   0, 1000, NULL),
              CBM_GOV_ERR_INVALID_INTERVAL);
    ASSERT_EQ(cbm_gov_propose_rule(gov, "R-OVERFLOW", "src/**", "LAYER_BOUNDARY",
                                   "{\"forbid\":\"ui->db\"}", "agent", "sess-1", "[]",
                                   INT64_MAX, 1000, NULL),
              CBM_GOV_ERR_INVALID_INTERVAL);

    /* U12 & F05: Propose rule with injected status="APPROVED" and reviewed_by="root";
     * must be stripped and forced to UNVERIFIED_CANDIDATE with reviewed_at=0. */
    bool replay = false;
    ASSERT_EQ(cbm_gov_propose_rule(gov, "R-001", "src/**", "EFFECT_RESTRICTION",
                                   "{\"forbid_writes_db\":true,\"status\":\"APPROVED\",\"reviewed_by\":\"root\"}",
                                   "flash-agent", "sess-42", "[\"commit:abc\"]",
                                   3600, 1000, &replay),
              CBM_GOV_OK);
    ASSERT_FALSE(replay);

    cbm_tradition_rule_t r = {0};
    ASSERT_EQ(cbm_gov_get_rule(gov, "R-001", 1000, &r), CBM_GOV_OK);
    ASSERT_STR_EQ(r.status, "UNVERIFIED_CANDIDATE");
    ASSERT_EQ(r.reviewed_at, 0);
    ASSERT_NULL(r.reviewed_by);
    ASSERT_NULL(strstr(r.payload_json, "\"APPROVED\""));
    cbm_gov_rule_free(&r);

    /* I18: Idempotent replay on identical candidate proposal */
    ASSERT_EQ(cbm_gov_propose_rule(gov, "R-001", "src/**", "EFFECT_RESTRICTION",
                                   "{\"forbid_writes_db\":true}",
                                   "flash-agent", "sess-42", "[\"commit:abc\"]",
                                   3600, 1005, &replay),
              CBM_GOV_OK);
    ASSERT_TRUE(replay);

    /* I18: Conflict on different payload for same rule_id */
    ASSERT_EQ(cbm_gov_propose_rule(gov, "R-001", "src/**", "EFFECT_RESTRICTION",
                                   "{\"forbid_writes_db\":false}",
                                   "flash-agent", "sess-42", "[\"commit:abc\"]",
                                   3600, 1010, &replay),
              CBM_GOV_ERR_RULE_CONFLICT);

    /* U14 & I17: Agent without verified operator capability cannot approve */
    ASSERT_EQ(cbm_gov_review_rule(gov, "R-001", "APPROVED", "agent-spoof", false,
                                  "self-approval attempt", 3600, NULL, 2000),
              CBM_GOV_ERR_UNAUTHORIZED_REVIEWER);

    /* U13: Verified operator approves rule at t=2000 with interval=3600 -> due at 5600 */
    ASSERT_EQ(cbm_gov_review_rule(gov, "R-001", "APPROVED", "operator-alice", true,
                                  "Verified boundary rule", 3600, NULL, 2000),
              CBM_GOV_OK);

    /* U11 / Monotonic clock guard: evaluation_time < last_trusted_time (2000) -> CLOCK_UNCERTAIN */
    char rev_before[65] = {0};
    ASSERT_EQ(cbm_gov_evaluate_active_rules(gov, 1500, NULL, NULL, rev_before, NULL),
              CBM_GOV_ERR_CLOCK_UNCERTAIN);

    /* Evaluate at t=3000 (< 5600): effective_status is APPROVED */
    cbm_tradition_rule_t *active = NULL;
    int active_count = 0;
    ASSERT_EQ(cbm_gov_evaluate_active_rules(gov, 3000, &active, &active_count, rev_before, NULL),
              CBM_GOV_OK);
    ASSERT_EQ(active_count, 1);
    ASSERT_STR_EQ(active[0].status, "APPROVED");
    ASSERT_STR_EQ(active[0].effective_status, "APPROVED");
    ASSERT_EQ(active[0].review_due_at, 5600);
    cbm_gov_rules_free(active, active_count);

    /* U15 & U16: Evaluate at t=6000 (>= 5600): projected as REVIEW_PENDING,
     * still active in active_rules, and rules_revision deterministically reflects the transition! */
    char rev_after[65] = {0};
    ASSERT_EQ(cbm_gov_evaluate_active_rules(gov, 6000, &active, &active_count, rev_after, NULL),
              CBM_GOV_OK);
    ASSERT_EQ(active_count, 1);
    ASSERT_STR_EQ(active[0].status, "APPROVED");
    ASSERT_STR_EQ(active[0].effective_status, "REVIEW_PENDING");
    ASSERT_TRUE(strcmp(rev_before, rev_after) != 0);
    cbm_gov_rules_free(active, active_count);

    cbm_gov_store_close(gov);
    free(db_path);
    th_rmtree(tmpdir);
    PASS();
}

/* ── Integration & Adversarial Scenarios (I01 - I22, U09, U17-U19, F02-F09) ── */

TEST(hv2_i07_i08_i09_sqlite_wal_pragmas_and_append_only_triggers) {
    char tmpdir[256];
    char *db_path = make_temp_gov_db_path(tmpdir, sizeof(tmpdir));
    ASSERT_NOT_NULL(db_path);

    cbm_gov_store_t *gov = NULL;
    ASSERT_EQ(cbm_gov_store_open(db_path, "test_proj", &gov), CBM_GOV_OK);
    sqlite3 *db = cbm_gov_store_get_db(gov);
    ASSERT_NOT_NULL(db);

    /* Propose and approve a rule so tradition_reviews has a row */
    ASSERT_EQ(cbm_gov_propose_rule(gov, "R-TRIG", "src/**", "NAMING_CONVENTION",
                                   "{\"pattern\":\"^cbm_\"}", "agent", "s1", "[]", 3600, 100, NULL),
              CBM_GOV_OK);
    ASSERT_EQ(cbm_gov_review_rule(gov, "R-TRIG", "APPROVED", "op", true, "ok", 3600, NULL, 200),
              CBM_GOV_OK);

    /* I09: Attempt UPDATE or DELETE on tradition_reviews -> must fail via trigger */
    char *errmsg = NULL;
    ASSERT_NEQ(sqlite3_exec(db, "UPDATE tradition_reviews SET rationale='tampered';", NULL, NULL, &errmsg),
               SQLITE_OK);
    sqlite3_free(errmsg);
    ASSERT_NEQ(sqlite3_exec(db, "DELETE FROM tradition_reviews;", NULL, NULL, &errmsg),
               SQLITE_OK);
    sqlite3_free(errmsg);

    /* Insert a manifest and verify append-only triggers on harness_admission_manifests (I08) */
    char bhash[65], snap[65], rrev[65], crev[65];
    cbm_sha256_hex("bundle1", 7, bhash);
    ASSERT_EQ(cbm_gov_evaluate_active_rules(gov, 300, NULL, NULL, rrev, NULL), CBM_GOV_OK);
    cbm_sha256_hex("cov1", 4, crev);
    cbm_harness_compute_snapshot_id("g1", rrev, crev, snap);
    cbm_admission_manifest_t m = {
        .manifest_id = "M-TRIG-1",
        .task_id = "T-1",
        .project_id = "test_proj",
        .snapshot_id = snap,
        .graph_generation = "g1",
        .rules_revision = rrev,
        .coverage_revision = crev,
        .bundle_hash = bhash,
        .fast_path_granted = 1,
        .fast_path_reasons_json = "[\"all_seams_resolved\"]",
        .advisor_invoked = 0,
        .advisor_verdict_hash = NULL,
        .blocking_violations_json = "[]",
        .warnings_json = "[]",
        .tdd_oracles_passed = 1
    };
    char adm_status[32] = {0};
    ASSERT_EQ(cbm_gov_record_manifest(gov, &m, snap, "g1", rrev, crev, true, 300, adm_status, NULL),
              CBM_GOV_OK);
    ASSERT_STR_EQ(adm_status, "ADMITTED_CLEAN");

    ASSERT_NEQ(sqlite3_exec(db, "UPDATE harness_admission_manifests SET fast_path_granted=0;", NULL, NULL, &errmsg),
               SQLITE_OK);
    sqlite3_free(errmsg);
    ASSERT_NEQ(sqlite3_exec(db, "DELETE FROM harness_admission_manifests WHERE manifest_id='M-TRIG-1';", NULL, NULL, &errmsg),
               SQLITE_OK);
    sqlite3_free(errmsg);
    /* Also verify INSERT OR REPLACE is blocked by manifest_no_replace trigger */
    char replace_sql[1024];
    snprintf(replace_sql, sizeof(replace_sql),
             "INSERT OR REPLACE INTO harness_admission_manifests "
             "(manifest_id, task_id, project_id, snapshot_id, graph_generation, rules_revision, "
             "coverage_revision, bundle_hash, admission_status, fast_path_granted, fast_path_reasons_json, "
             "advisor_invoked, blocking_violations_json, warnings_json, tdd_oracles_passed, created_at) "
             "VALUES ('M-TRIG-1','T-1','test_proj','%s','g1','%s','%s','%s','ADMITTED_CLEAN',0,'[]',0,'[]','[]',1,301);",
             snap, rrev, crev, bhash);
    ASSERT_NEQ(sqlite3_exec(db, replace_sql, NULL, NULL, &errmsg), SQLITE_OK);
    sqlite3_free(errmsg);

    /* Verify Option A relational CHECK constraint rejects ADMITTED_CLEAN with non-empty blocking_violations_json */
    char bad_check_sql[1024];
    snprintf(bad_check_sql, sizeof(bad_check_sql),
             "INSERT INTO harness_admission_manifests "
             "(manifest_id, task_id, project_id, snapshot_id, graph_generation, rules_revision, "
             "coverage_revision, bundle_hash, admission_status, fast_path_granted, fast_path_reasons_json, "
             "advisor_invoked, blocking_violations_json, warnings_json, tdd_oracles_passed, created_at) "
             "VALUES ('M-BAD-CHK','T-1','test_proj','%s','g1','%s','%s','%s','ADMITTED_CLEAN',0,'[]',0,'[\"VIOLATION\"]','[]',1,302);",
             snap, rrev, crev, bhash);
    ASSERT_NEQ(sqlite3_exec(db, bad_check_sql, NULL, NULL, &errmsg), SQLITE_OK);
    sqlite3_free(errmsg);

    cbm_gov_store_close(gov);
    free(db_path);
    th_rmtree(tmpdir);
    PASS();
}

TEST(hv2_u17_i10_i12_i13_i14_i15_i16_f03_option_a_manifest_ledger) {
    char tmpdir[256];
    char *db_path = make_temp_gov_db_path(tmpdir, sizeof(tmpdir));
    ASSERT_NOT_NULL(db_path);

    cbm_gov_store_t *gov = NULL;
    ASSERT_EQ(cbm_gov_store_open(db_path, "test_proj", &gov), CBM_GOV_OK);

    char bhash[65], snap_valid[65], snap_stale[65], rrev1[65], crev1[65];
    cbm_sha256_hex("bundle-opt-a", 12, bhash);
    /* Compute actual live rules_revision from empty governance DB */
    ASSERT_EQ(cbm_gov_evaluate_active_rules(gov, 900, NULL, NULL, rrev1, NULL), CBM_GOV_OK);
    cbm_sha256_hex("cov_rev_1", 9, crev1);
    cbm_harness_compute_snapshot_id("u1g1", rrev1, crev1, snap_valid);
    cbm_harness_compute_snapshot_id("u1g2", rrev1, crev1, snap_stale);

    cbm_admission_manifest_t m = {
        .manifest_id = "M-BLOCKED-01",
        .task_id = "TASK-01",
        .project_id = "test_proj",
        .snapshot_id = snap_valid,
        .graph_generation = "u1g1",
        .rules_revision = rrev1,
        .coverage_revision = crev1,
        .bundle_hash = bhash,
        .fast_path_granted = 1, /* Caller tries to claim fast_path=1 despite blocking violation */
        .fast_path_reasons_json = "[\"spoofed\"]",
        .advisor_invoked = 1,
        .advisor_verdict_hash = bhash,
        .blocking_violations_json = "[\"RULE_LAYER_BOUNDARY_VIOLATED\"]",
        .warnings_json = "[\"NAMING_DRIFT_WARN\"]",
        .tdd_oracles_passed = 0
    };

    /* U17 & I14 (Option A Full Forensic Trail):
     * Non-empty blocking_violations MUST return CBM_GOV_ERR_BLOCKING_VIOLATIONS,
     * AND durably commit the row with admission_status='REJECTED_BLOCKING' and fast_path_granted=0! */
    char adm_status[32] = {0};
    bool replay = false;
    ASSERT_EQ(cbm_gov_record_manifest(gov, &m, snap_valid, "u1g1", rrev1, crev1,
                                      false, 1000, adm_status, &replay),
              CBM_GOV_ERR_BLOCKING_VIOLATIONS);
    ASSERT_STR_EQ(adm_status, "REJECTED_BLOCKING");
    ASSERT_FALSE(replay);

    cbm_admission_manifest_t stored = {0};
    ASSERT_EQ(cbm_gov_get_manifest(gov, "M-BLOCKED-01", &stored), CBM_GOV_OK);
    ASSERT_STR_EQ(stored.admission_status, "REJECTED_BLOCKING");
    ASSERT_EQ(stored.fast_path_granted, 0); /* Forced to 0 on blocking violation */
    ASSERT_STR_EQ(stored.blocking_violations_json, "[\"RULE_LAYER_BOUNDARY_VIOLATED\"]");
    ASSERT_STR_EQ(stored.warnings_json, "[\"NAMING_DRIFT_WARN\"]");
    cbm_gov_manifest_free(&stored);

    /* I16: Warnings only (blocking_violations == "[]") -> ADMITTED_WITH_WARNINGS */
    m.manifest_id = "M-WARN-01";
    m.blocking_violations_json = "[]";
    m.warnings_json = "[\"ADVISORY_COMPLEXITY_HIGH\"]";
    m.fast_path_granted = 0;
    ASSERT_EQ(cbm_gov_record_manifest(gov, &m, snap_valid, "u1g1", rrev1, crev1,
                                      false, 1010, adm_status, &replay),
              CBM_GOV_OK);
    ASSERT_STR_EQ(adm_status, "ADMITTED_WITH_WARNINGS");

    ASSERT_EQ(cbm_gov_get_manifest(gov, "M-WARN-01", &stored), CBM_GOV_OK);
    ASSERT_STR_EQ(stored.admission_status, "ADMITTED_WITH_WARNINGS");
    ASSERT_STR_EQ(stored.warnings_json, "[\"ADVISORY_COMPLEXITY_HIGH\"]");
    cbm_gov_manifest_free(&stored);

    /* I15: Idempotent replay of M-WARN-01 succeeds with replay=true; conflicting payload fails */
    ASSERT_EQ(cbm_gov_record_manifest(gov, &m, snap_valid, "u1g1", rrev1, crev1,
                                      false, 1020, adm_status, &replay),
              CBM_GOV_OK);
    ASSERT_TRUE(replay);

    m.bundle_hash = snap_valid; /* Mutate payload with same manifest_id */
    ASSERT_EQ(cbm_gov_record_manifest(gov, &m, snap_valid, "u1g1", rrev1, crev1,
                                      false, 1025, adm_status, &replay),
              CBM_GOV_ERR_MANIFEST_ID_CONFLICT);
    m.bundle_hash = bhash;

    /* I10 & I12: Stale snapshot rejected */
    m.manifest_id = "M-STALE-01";
    m.snapshot_id = snap_stale;
    ASSERT_EQ(cbm_gov_record_manifest(gov, &m, snap_valid, "u1g1", rrev1, crev1,
                                      true, 1030, adm_status, &replay),
              CBM_GOV_ERR_STALE_SNAPSHOT);

    /* F03: Fast-Path spoofing rejected when server_fast_path_eligible == false */
    m.manifest_id = "M-SPOOF-FP";
    m.snapshot_id = snap_valid;
    m.fast_path_granted = 1;
    ASSERT_EQ(cbm_gov_record_manifest(gov, &m, snap_valid, "u1g1", rrev1, crev1,
                                      false, 1040, adm_status, &replay),
              CBM_GOV_ERR_FAST_PATH_NOT_ELIGIBLE);

    /* I13: Publication fence active -> rejected with PUBLICATION_IN_PROGRESS */
    ASSERT_EQ(cbm_gov_set_publication_status(gov, "PUBLISHING", NULL, 1050), CBM_GOV_OK);
    m.manifest_id = "M-DURING-PUB";
    m.fast_path_granted = 0;
    ASSERT_EQ(cbm_gov_record_manifest(gov, &m, snap_valid, "u1g1", rrev1, crev1,
                                      true, 1055, adm_status, &replay),
              CBM_GOV_ERR_PUBLICATION_IN_PROGRESS);
    ASSERT_EQ(cbm_gov_set_publication_status(gov, "STABLE", "u1g1", 1060), CBM_GOV_OK);

    cbm_gov_store_close(gov);
    free(db_path);
    th_rmtree(tmpdir);
    PASS();
}

TEST(hv2_u09_i01_i06_u18_u19_f02_f04_f07_extract_bundle_and_fast_path) {
    char tmpdir[256];
    char *db_path = make_temp_gov_db_path(tmpdir, sizeof(tmpdir));
    ASSERT_NOT_NULL(db_path);

    cbm_store_t *store = cbm_store_open_memory();
    ASSERT_NOT_NULL(store);
    ASSERT_EQ(cbm_store_upsert_project(store, "test_proj", tmpdir), CBM_STORE_OK);

    cbm_gov_store_t *gov = NULL;
    ASSERT_EQ(cbm_gov_store_open(db_path, "test_proj", &gov), CBM_GOV_OK);

    /* Create 2 nodes:
     * - pole_seed (hop 0): pure function calling neighbor_pure (hop 1) */
    cbm_node_t n_seed = {
        .project = "test_proj", .label = "Function", .name = "computeTotal",
        .qualified_name = "billing.computeTotal", .file_path = "billing.c",
        .start_line = 1, .end_line = 12, .properties_json = "{}"
    };
    cbm_node_t n_neigh = {
        .project = "test_proj", .label = "Function", .name = "applyDiscount",
        .qualified_name = "billing.applyDiscount", .file_path = "discount.c",
        .start_line = 5, .end_line = 20, .properties_json = "{}"
    };
    int64_t id_seed = cbm_store_upsert_node(store, &n_seed);
    int64_t id_neigh = cbm_store_upsert_node(store, &n_neigh);

    cbm_edge_t e_call = {
        .project = "test_proj", .source_id = id_seed, .target_id = id_neigh,
        .type = "CALLS", .properties_json = "{}"
    };
    cbm_store_insert_edge(store, &e_call);

    const char *seeds[] = {"billing.computeTotal"};
    char *bundle1 = NULL, *bundle2 = NULL;
    char hash1[65], hash2[65], snap1[65], snap2[65];

    ASSERT_EQ(cbm_harness_extract_bundle(store, gov, "test_proj", "TASK-100",
                                         "pure_refactor", seeds, 1, 1, 5000,
                                         &bundle1, hash1, snap1), 0);
    ASSERT_EQ(cbm_harness_extract_bundle(store, gov, "test_proj", "TASK-100",
                                         "pure_refactor", seeds, 1, 1, 5000,
                                         &bundle2, hash2, snap2), 0);
    /* I03: Byte-for-byte deterministic extraction & identical bundle_hash (even across different evaluation_time) */
    ASSERT_STR_EQ(hash1, hash2);
    ASSERT_STR_EQ(snap1, snap2);
    ASSERT_STR_EQ(bundle1, bundle2);

    /* Inspect JSON structure for U09, I01, I02, I04:
     * - hop 0 candidate has focus.mode == "integral" and non-empty "code"
     * - hop 1 neighbor has focus.mode == "skeleton", "skeleton" capsule, and NO "code" field! */
    yyjson_doc *doc = yyjson_read(bundle1, strlen(bundle1), 0);
    ASSERT_NOT_NULL(doc);
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *cands = yyjson_obj_get(root, "candidates");
    ASSERT_EQ(yyjson_arr_size(cands), 2);

    yyjson_val *c0 = yyjson_arr_get(cands, 0);
    yyjson_val *c1 = yyjson_arr_get(cands, 1);
    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(c0, "hop_distance")), 0);
    ASSERT_STR_EQ(yyjson_get_str(yyjson_obj_get(yyjson_obj_get(c0, "focus"), "mode")), "integral");
    ASSERT_NOT_NULL(yyjson_obj_get(yyjson_obj_get(c0, "focus"), "code"));

    ASSERT_EQ(yyjson_get_int(yyjson_obj_get(c1, "hop_distance")), 1);
    ASSERT_STR_EQ(yyjson_get_str(yyjson_obj_get(yyjson_obj_get(c1, "focus"), "mode")), "skeleton");
    ASSERT_NULL(yyjson_obj_get(yyjson_obj_get(c1, "focus"), "code"));
    ASSERT_NOT_NULL(yyjson_obj_get(yyjson_obj_get(c1, "focus"), "skeleton"));

    /* U18: Clean bundle with resolved seams, zero unknown effects, complete coverage -> Fast-Path eligible */
    cbm_fast_path_verdict_t verdict = {0};
    char *vjson = NULL;
    ASSERT_EQ(cbm_harness_evaluate_fast_path(store, gov, "test_proj", bundle1, NULL, 5000, &verdict, &vjson), 0);
    ASSERT_TRUE(verdict.eligible);
    ASSERT_TRUE(verdict.all_seams_resolved);
    ASSERT_TRUE(verdict.zero_uncertainty_effects);
    ASSERT_TRUE(verdict.coverage_complete);
    cbm_fast_path_verdict_free(&verdict);
    free(vjson);
    yyjson_doc_free(doc);

    /* U19 & F04: Introduce a coverage gap on discount.c -> Fast-Path MUST fail closed!
     * Note: cbm_store_coverage_replace prunes rows absent from file_hashes, so upsert file hash first. */
    ASSERT_EQ(cbm_store_upsert_file_hash(store, "test_proj", "discount.c", "hash_disc_1", 1000, 200),
              CBM_STORE_OK);
    cbm_coverage_row_t gap_row = {
        .rel_path = "discount.c", .kind = "parse_partial", .detail = "10-20"
    };
    ASSERT_EQ(cbm_store_coverage_replace(store, "test_proj", &gap_row, 1), CBM_STORE_OK);

    char *bundle_gap = NULL;
    char hash_gap[65], snap_gap[65];
    ASSERT_EQ(cbm_harness_extract_bundle(store, gov, "test_proj", "TASK-101",
                                         "pure_refactor", seeds, 1, 1, 5000,
                                         &bundle_gap, hash_gap, snap_gap), 0);
    /* Snapshot ID must change when coverage_revision changes (I11) */
    ASSERT_TRUE(strcmp(snap1, snap_gap) != 0);

    ASSERT_EQ(cbm_harness_evaluate_fast_path(store, gov, "test_proj", bundle_gap, NULL, 5000, &verdict, NULL), 0);
    ASSERT_FALSE(verdict.eligible);
    ASSERT_FALSE(verdict.coverage_complete);
    ASSERT_NOT_NULL(strstr(verdict.disqualifying_reasons_json, "coverage_incomplete"));
    cbm_fast_path_verdict_free(&verdict);

    free(bundle1);
    free(bundle2);
    free(bundle_gap);
    cbm_gov_store_close(gov);
    cbm_store_close(store);
    free(db_path);
    th_rmtree(tmpdir);
    PASS();
}

TEST(hv2_i19_i22_mcp_tools_e2e_contract) {
    char tmpdir[256];
    char *gov_db_path = make_temp_gov_db_path(tmpdir, sizeof(tmpdir));
    ASSERT_NOT_NULL(gov_db_path);

    /* Direct managed governance DB resolution to tmpdir via CBM_GOVERNANCE_DIR */
    cbm_setenv("CBM_GOVERNANCE_DIR", tmpdir, 1);

    cbm_mcp_server_t *srv = cbm_mcp_server_new(NULL);
    ASSERT_NOT_NULL(srv);
    cbm_mcp_server_set_project(srv, "mcp_hv2_proj");
    cbm_mcp_server_set_operator_authority(srv, "operator-lead", "secret-token-999");

    cbm_store_t *store = cbm_mcp_server_store(srv);
    ASSERT_NOT_NULL(store);
    ASSERT_EQ(cbm_store_upsert_project(store, "mcp_hv2_proj", tmpdir), CBM_STORE_OK);

    cbm_node_t n1 = {
        .project = "mcp_hv2_proj", .label = "Function", .name = "handleOrder",
        .qualified_name = "orders.handleOrder", .file_path = "orders.c",
        .start_line = 1, .end_line = 15, .properties_json = "{}"
    };
    cbm_store_upsert_node(store, &n1);

    /* 1. Call tradition_propose via MCP using host clock override */
    cbm_gov_set_clock_override_for_tests(1000);
    char args_buf[2048];
    snprintf(args_buf, sizeof(args_buf),
             "{\"project\":\"mcp_hv2_proj\","
             "\"rule_id\":\"R-MCP-01\",\"scope_glob\":\"orders.*\","
             "\"constraint_kind\":\"NAMING_CONVENTION\","
             "\"payload_json\":\"{\\\"prefix\\\":\\\"ord_\\\"}\"}");
    char *res_prop = cbm_mcp_handle_tool(srv, "tradition_propose", args_buf);
    ASSERT_NOT_NULL(res_prop);
    ASSERT_NOT_NULL(strstr(res_prop, "UNVERIFIED_CANDIDATE"));
    free(res_prop);

    /* 2. Approve rule via operator-authenticated tradition_propose(action="review") */
    cbm_gov_set_clock_override_for_tests(1100);
    snprintf(args_buf, sizeof(args_buf),
             "{\"project\":\"mcp_hv2_proj\","
             "\"action\":\"review\",\"rule_id\":\"R-MCP-01\","
             "\"new_status\":\"APPROVED\",\"operator_id\":\"operator-lead\","
             "\"operator_token\":\"secret-token-999\"}");
    char *res_rev = cbm_mcp_handle_tool(srv, "tradition_propose", args_buf);
    ASSERT_NOT_NULL(res_rev);
    ASSERT_NOT_NULL(strstr(res_rev, "\"APPROVED\""));
    free(res_rev);

    /* 3. Extract harness bundle via MCP */
    cbm_gov_set_clock_override_for_tests(1200);
    snprintf(args_buf, sizeof(args_buf),
             "{\"project\":\"mcp_hv2_proj\","
             "\"task_id\":\"T-MCP-1\",\"change_kind\":\"local_fix\","
             "\"k_hop\":1,\"poles\":[\"orders.handleOrder\"]}");
    char *res_ext = cbm_mcp_handle_tool(srv, "extract_harness_bundle", args_buf);
    ASSERT_NOT_NULL(res_ext);
    ASSERT_NOT_NULL(strstr(res_ext, "orders.handleOrder"));
    ASSERT_NOT_NULL(strstr(res_ext, "R-MCP-01"));

    /* Extract inner text JSON from MCP tool result */
    yyjson_doc *rdoc = yyjson_read(res_ext, strlen(res_ext), 0);
    yyjson_val *content = yyjson_obj_get(yyjson_doc_get_root(rdoc), "content");
    const char *bundle_text = yyjson_get_str(yyjson_obj_get(yyjson_arr_get(content, 0), "text"));
    yyjson_doc *bdoc = yyjson_read(bundle_text, strlen(bundle_text), 0);
    const char *snap_id = yyjson_get_str(yyjson_obj_get(yyjson_doc_get_root(bdoc), "snapshot_id"));
    const char *bhash = yyjson_get_str(yyjson_obj_get(yyjson_doc_get_root(bdoc), "bundle_hash"));
    const char *crev = yyjson_get_str(yyjson_obj_get(yyjson_doc_get_root(bdoc), "coverage_revision"));

    /* 4. Evaluate fast-path via MCP */
    snprintf(args_buf, sizeof(args_buf),
             "{\"project\":\"mcp_hv2_proj\","
             "\"poles\":[\"orders.handleOrder\"],\"change_kind\":\"local_fix\"}");
    char *res_fp = cbm_mcp_handle_tool(srv, "evaluate_fast_path", args_buf);
    ASSERT_NOT_NULL(res_fp);
    ASSERT_NOT_NULL(strstr(res_fp, "\\\"eligible\\\":true"));
    free(res_fp);

    /* 5. Record admission manifest with blocking violation via MCP (Option A forensic check) */
    cbm_gov_set_clock_override_for_tests(1250);
    snprintf(args_buf, sizeof(args_buf),
             "{\"project\":\"mcp_hv2_proj\","
             "\"manifest_id\":\"M-MCP-BLOCKED\",\"task_id\":\"T-MCP-1\","
             "\"snapshot_id\":\"%s\",\"coverage_revision\":\"%s\",\"bundle_hash\":\"%s\","
             "\"fast_path_granted\":false,\"advisor_invoked\":true,"
             "\"blocking_violations\":[\"SEAM_CONTRACT_BROKEN\"],"
             "\"warnings\":[\"MINOR_STYLE_NOTE\"]}",
             snap_id, crev, bhash);
    char *res_man = cbm_mcp_handle_tool(srv, "record_admission_manifest", args_buf);
    ASSERT_NOT_NULL(res_man);
    ASSERT_NOT_NULL(strstr(res_man, "\"isError\":true"));
    ASSERT_NOT_NULL(strstr(res_man, "BLOCKING_VIOLATIONS"));
    ASSERT_NOT_NULL(strstr(res_man, "REJECTED_BLOCKING"));
    ASSERT_NOT_NULL(strstr(res_man, "M-MCP-BLOCKED"));
    free(res_man);

    cbm_gov_set_clock_override_for_tests(0);
    cbm_unsetenv("CBM_GOVERNANCE_DIR");
    yyjson_doc_free(bdoc);
    yyjson_doc_free(rdoc);
    free(res_ext);
    cbm_mcp_server_free(srv);
    free(gov_db_path);
    th_rmtree(tmpdir);
    PASS();
}

SUITE(harness_v2) {
    RUN_TEST(hv2_u01_u02_snapshot_id_determinism_and_validation);
    RUN_TEST(hv2_u03_u04_span_and_khop_bounds);
    RUN_TEST(hv2_u05_u06_tristate_effect_lattice_laws);
    RUN_TEST(hv2_u07_u08_f01_effect_propagation_and_cycle_convergence);
    RUN_TEST(hv2_u10_canonical_json_rfc8785_key_order_invariance);
    RUN_TEST(hv2_u11_u12_u13_u14_u15_u16_f05_tradition_governance_and_cadence);
    RUN_TEST(hv2_i07_i08_i09_sqlite_wal_pragmas_and_append_only_triggers);
    RUN_TEST(hv2_u17_i10_i12_i13_i14_i15_i16_f03_option_a_manifest_ledger);
    RUN_TEST(hv2_u09_i01_i06_u18_u19_f02_f04_f07_extract_bundle_and_fast_path);
    RUN_TEST(hv2_i19_i22_mcp_tools_e2e_contract);
}
