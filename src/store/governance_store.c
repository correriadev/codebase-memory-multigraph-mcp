#include "store/governance_store.h"
#include "harness/harness_v2.h"
#include "foundation/sha256.h"
#include "foundation/platform.h"
#include "foundation/log.h"
#include "yyjson/yyjson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

struct cbm_gov_store {
    sqlite3 *db;
    char *db_path;
    char *project_id;
    char last_error[512];
};

static char *gov_strdup(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

static void gov_set_error(cbm_gov_store_t *store, const char *msg) {
    if (!store) {
        return;
    }
    if (!msg) {
        store->last_error[0] = '\0';
        return;
    }
    snprintf(store->last_error, sizeof(store->last_error), "%s", msg);
}

const char *cbm_gov_store_last_error(const cbm_gov_store_t *store) {
    return store ? store->last_error : "";
}

sqlite3 *cbm_gov_store_get_db(cbm_gov_store_t *store) {
    return store ? store->db : NULL;
}

const char *cbm_gov_status_code_str(cbm_gov_status_t status) {
    switch (status) {
    case CBM_GOV_OK:
        return "OK";
    case CBM_GOV_ERR_PRAGMA_MISMATCH:
        return "PRAGMA_MISMATCH";
    case CBM_GOV_ERR_UNAUTHORIZED_REVIEWER:
        return "UNAUTHORIZED_REVIEWER";
    case CBM_GOV_ERR_RULE_CONFLICT:
        return "RULE_CONFLICT";
    case CBM_GOV_ERR_RULE_NOT_FOUND:
        return "RULE_NOT_FOUND";
    case CBM_GOV_ERR_CLOCK_UNCERTAIN:
        return "CLOCK_UNCERTAIN";
    case CBM_GOV_ERR_INVALID_PAYLOAD:
        return "INVALID_PAYLOAD";
    case CBM_GOV_ERR_INVALID_INTERVAL:
        return "INVALID_INTERVAL";
    case CBM_GOV_ERR_PUBLICATION_IN_PROGRESS:
        return "PUBLICATION_IN_PROGRESS";
    case CBM_GOV_ERR_STALE_SNAPSHOT:
        return "STALE_SNAPSHOT";
    case CBM_GOV_ERR_MANIFEST_ID_CONFLICT:
        return "MANIFEST_ID_CONFLICT";
    case CBM_GOV_ERR_FAST_PATH_NOT_ELIGIBLE:
        return "FAST_PATH_NOT_ELIGIBLE";
    case CBM_GOV_ERR_BLOCKING_VIOLATIONS:
        return "BLOCKING_VIOLATIONS";
    default:
        return "GOVERNANCE_ERROR";
    }
}

char *cbm_gov_resolve_db_path(const char *project_id, const char *custom_dir) {
    const char *safe_proj = (project_id && project_id[0]) ? project_id : "default";
    const char *dir = custom_dir;
    if (!dir || !dir[0]) {
        dir = getenv("CBM_GOVERNANCE_DIR");
    }
    if (!dir || !dir[0]) {
        dir = cbm_resolve_cache_dir();
    }
    if (!dir || !dir[0]) {
        dir = ".";
    }
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
    size_t len = strlen(dir) + strlen(safe_proj) + 32;
    char *path = (char *)malloc(len);
    if (!path) {
        return NULL;
    }
    snprintf(path, len, "%s/%s.governance.db", dir, safe_proj);
    return path;
}

static int64_t g_gov_clock_override = 0;

void cbm_gov_set_clock_override_for_tests(int64_t override_epoch) {
    g_gov_clock_override = override_epoch;
}

int64_t cbm_gov_now(void) {
    return (g_gov_clock_override > 0) ? g_gov_clock_override : (int64_t)time(NULL);
}

static const char *GOV_SCHEMA_DDL =
    "CREATE TABLE IF NOT EXISTS governance_state ("
    "  singleton_id INTEGER PRIMARY KEY CHECK (singleton_id = 1),"
    "  project_id TEXT NOT NULL,"
    "  rules_revision TEXT NOT NULL,"
    "  last_known_graph_generation TEXT NOT NULL,"
    "  publication_status TEXT NOT NULL CHECK(publication_status IN ('STABLE', 'PUBLISHING', 'RECOVERY_REQUIRED')),"
    "  last_trusted_time INTEGER NOT NULL,"
    "  updated_at INTEGER NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS tradition_rules ("
    "  rule_id TEXT PRIMARY KEY NOT NULL CHECK(length(rule_id) > 0),"
    "  scope_glob TEXT NOT NULL CHECK(length(scope_glob) > 0),"
    "  constraint_kind TEXT NOT NULL CHECK(constraint_kind IN ('FORBIDDEN_IMPORT', 'FORBIDDEN_CALL', 'REQUIRED_SEAM_CONTRACT', 'NAMING_CONVENTION', 'LAYER_BOUNDARY', 'EFFECT_RESTRICTION')),"
    "  payload_json TEXT NOT NULL CHECK(json_valid(payload_json)),"
    "  payload_hash TEXT NOT NULL CHECK(length(payload_hash) = 64),"
    "  status TEXT NOT NULL CHECK(status IN ('UNVERIFIED_CANDIDATE', 'APPROVED', 'REJECTED', 'SUPERSEDED')),"
    "  proposer_id TEXT NOT NULL,"
    "  origin_session_id TEXT NOT NULL,"
    "  evidence_refs_json TEXT NOT NULL CHECK(json_valid(evidence_refs_json) AND json_type(evidence_refs_json) = 'array'),"
    "  created_at INTEGER NOT NULL,"
    "  reviewed_at INTEGER NOT NULL DEFAULT 0 CHECK(reviewed_at >= 0),"
    "  reviewed_by TEXT,"
    "  review_interval_seconds INTEGER NOT NULL CHECK(review_interval_seconds > 0),"
    "  review_due_at INTEGER NOT NULL DEFAULT 0 CHECK(review_due_at >= 0),"
    "  superseded_by TEXT REFERENCES tradition_rules(rule_id)"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_tradition_rules_status_scope ON tradition_rules(status, scope_glob);"
    "CREATE INDEX IF NOT EXISTS idx_tradition_rules_due ON tradition_rules(status, review_due_at);"
    "CREATE TABLE IF NOT EXISTS tradition_reviews ("
    "  review_id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  rule_id TEXT NOT NULL REFERENCES tradition_rules(rule_id),"
    "  previous_status TEXT NOT NULL,"
    "  new_status TEXT NOT NULL,"
    "  reviewer_id TEXT NOT NULL,"
    "  rationale TEXT NOT NULL,"
    "  reviewed_at INTEGER NOT NULL"
    ");"
    "CREATE TRIGGER IF NOT EXISTS review_no_update BEFORE UPDATE ON tradition_reviews "
    "BEGIN SELECT RAISE(ABORT, 'tradition_reviews is append-only'); END;"
    "CREATE TRIGGER IF NOT EXISTS review_no_delete BEFORE DELETE ON tradition_reviews "
    "BEGIN SELECT RAISE(ABORT, 'tradition_reviews is append-only'); END;"
    "CREATE TRIGGER IF NOT EXISTS review_no_replace BEFORE INSERT ON tradition_reviews "
    "WHEN NEW.review_id IS NOT NULL AND EXISTS(SELECT 1 FROM tradition_reviews WHERE review_id = NEW.review_id) "
    "BEGIN SELECT RAISE(ABORT, 'tradition_reviews is append-only'); END;"
    "CREATE TABLE IF NOT EXISTS harness_admission_manifests ("
    "  manifest_id TEXT PRIMARY KEY NOT NULL CHECK(length(manifest_id) > 0),"
    "  task_id TEXT NOT NULL CHECK(length(task_id) > 0),"
    "  project_id TEXT NOT NULL CHECK(length(project_id) > 0),"
    "  snapshot_id TEXT NOT NULL CHECK(length(snapshot_id) = 64),"
    "  graph_generation TEXT NOT NULL,"
    "  rules_revision TEXT NOT NULL,"
    "  coverage_revision TEXT NOT NULL,"
    "  bundle_hash TEXT NOT NULL CHECK(length(bundle_hash) = 64),"
    "  fast_path_granted INTEGER NOT NULL CHECK(fast_path_granted IN (0, 1)),"
    "  fast_path_reasons_json TEXT NOT NULL CHECK(json_valid(fast_path_reasons_json) AND json_type(fast_path_reasons_json) = 'array'),"
    "  advisor_invoked INTEGER NOT NULL CHECK(advisor_invoked IN (0, 1)),"
    "  advisor_verdict_hash TEXT,"
    "  blocking_violations_json TEXT NOT NULL CHECK(json_valid(blocking_violations_json) AND json_type(blocking_violations_json) = 'array'),"
    "  warnings_json TEXT NOT NULL CHECK(json_valid(warnings_json) AND json_type(warnings_json) = 'array'),"
    "  tdd_oracles_passed INTEGER NOT NULL CHECK(tdd_oracles_passed IN (0, 1)),"
    "  admission_status TEXT NOT NULL CHECK(admission_status IN ('ADMITTED_CLEAN', 'ADMITTED_WITH_WARNINGS', 'REJECTED_BLOCKING')),"
    "  created_at INTEGER NOT NULL,"
    "  CHECK((json_array_length(blocking_violations_json) = 0 AND json_array_length(warnings_json) = 0 AND admission_status = 'ADMITTED_CLEAN') OR "
    "        (json_array_length(blocking_violations_json) = 0 AND json_array_length(warnings_json) > 0 AND admission_status = 'ADMITTED_WITH_WARNINGS') OR "
    "        (json_array_length(blocking_violations_json) > 0 AND admission_status = 'REJECTED_BLOCKING' AND fast_path_granted = 0))"
    ");"
    "CREATE INDEX IF NOT EXISTS idx_manifests_project_task ON harness_admission_manifests(project_id, task_id);"
    "CREATE INDEX IF NOT EXISTS idx_manifests_snapshot ON harness_admission_manifests(snapshot_id);"
    "CREATE TRIGGER IF NOT EXISTS manifest_no_update BEFORE UPDATE ON harness_admission_manifests "
    "BEGIN SELECT RAISE(ABORT, 'harness_admission_manifests is append-only'); END;"
    "CREATE TRIGGER IF NOT EXISTS manifest_no_delete BEFORE DELETE ON harness_admission_manifests "
    "BEGIN SELECT RAISE(ABORT, 'harness_admission_manifests is append-only'); END;"
    "CREATE TRIGGER IF NOT EXISTS manifest_no_replace BEFORE INSERT ON harness_admission_manifests "
    "WHEN EXISTS(SELECT 1 FROM harness_admission_manifests WHERE manifest_id = NEW.manifest_id) "
    "BEGIN SELECT RAISE(ABORT, 'harness_admission_manifests is append-only'); END;";

static int verify_pragma_string(sqlite3 *db, const char *sql, const char *expected) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    int rc = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *val = (const char *)sqlite3_column_text(stmt, 0);
        if (val && expected) {
#ifdef _WIN32
            if (_stricmp(val, expected) == 0) {
                rc = 0;
            }
#else
            if (strcasecmp(val, expected) == 0) {
                rc = 0;
            }
#endif
        }
    }
    sqlite3_finalize(stmt);
    return rc;
}

static int verify_pragma_int(sqlite3 *db, const char *sql, int expected) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }
    int rc = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int val = sqlite3_column_int(stmt, 0);
        if (val == expected) {
            rc = 0;
        }
    }
    sqlite3_finalize(stmt);
    return rc;
}

cbm_gov_status_t cbm_gov_store_open(const char *db_path, const char *project_id,
                                    cbm_gov_store_t **out_store) {
    if (!out_store || !project_id || !project_id[0]) {
        return CBM_GOV_ERR_GENERIC;
    }
    *out_store = NULL;

    char *resolved_path = NULL;
    if (db_path && db_path[0]) {
        resolved_path = gov_strdup(db_path);
    } else {
        resolved_path = cbm_gov_resolve_db_path(project_id, NULL);
    }
    if (!resolved_path) {
        return CBM_GOV_ERR_GENERIC;
    }

    sqlite3 *db = NULL;
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(resolved_path, &db, flags, NULL) != SQLITE_OK) {
        if (db) {
            sqlite3_close(db);
        }
        free(resolved_path);
        return CBM_GOV_ERR_GENERIC;
    }

    bool is_memory = (strcmp(resolved_path, ":memory:") == 0);
    char *errmsg = NULL;
    if (!is_memory) {
        if (sqlite3_exec(db, "PRAGMA journal_mode = WAL;", NULL, NULL, &errmsg) != SQLITE_OK) {
            sqlite3_free(errmsg);
            sqlite3_close(db);
            free(resolved_path);
            return CBM_GOV_ERR_PRAGMA_MISMATCH;
        }
    }
    if (sqlite3_exec(db,
                     "PRAGMA synchronous = FULL;"
                     "PRAGMA foreign_keys = ON;"
                     "PRAGMA busy_timeout = 2000;",
                     NULL, NULL, &errmsg) != SQLITE_OK) {
        sqlite3_free(errmsg);
        sqlite3_close(db);
        free(resolved_path);
        return CBM_GOV_ERR_PRAGMA_MISMATCH;
    }

    /* Verify pragmas (Scenario I07) */
    if (!is_memory && verify_pragma_string(db, "PRAGMA journal_mode;", "wal") != 0) {
        sqlite3_close(db);
        free(resolved_path);
        return CBM_GOV_ERR_PRAGMA_MISMATCH;
    }
    /* synchronous FULL == 2 */
    if (verify_pragma_int(db, "PRAGMA synchronous;", 2) != 0) {
        sqlite3_close(db);
        free(resolved_path);
        return CBM_GOV_ERR_PRAGMA_MISMATCH;
    }
    if (verify_pragma_int(db, "PRAGMA foreign_keys;", 1) != 0) {
        sqlite3_close(db);
        free(resolved_path);
        return CBM_GOV_ERR_PRAGMA_MISMATCH;
    }

    if (sqlite3_exec(db, GOV_SCHEMA_DDL, NULL, NULL, &errmsg) != SQLITE_OK) {
        sqlite3_free(errmsg);
        sqlite3_close(db);
        free(resolved_path);
        return CBM_GOV_ERR_GENERIC;
    }

    /* Compute initial empty rules_revision: SHA256("[]") */
    char empty_rules_rev[65];
    cbm_sha256_hex("[]", 2, empty_rules_rev);

    /* Ensure singleton governance_state row exists */
    sqlite3_stmt *stmt = NULL;
    const char *init_sql =
        "INSERT OR IGNORE INTO governance_state "
        "(singleton_id, project_id, rules_revision, last_known_graph_generation, "
        " publication_status, last_trusted_time, updated_at) "
        "VALUES (1, ?1, ?2, '0', 'STABLE', 0, ?3);";
    if (sqlite3_prepare_v2(db, init_sql, -1, &stmt, NULL) == SQLITE_OK) {
        int64_t now_ts = (int64_t)time(NULL);
        sqlite3_bind_text(stmt, 1, project_id, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, empty_rules_rev, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, now_ts);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    cbm_gov_store_t *store = (cbm_gov_store_t *)calloc(1, sizeof(cbm_gov_store_t));
    if (!store) {
        sqlite3_close(db);
        free(resolved_path);
        return CBM_GOV_ERR_GENERIC;
    }
    store->db = db;
    store->db_path = resolved_path;
    store->project_id = gov_strdup(project_id);
    *out_store = store;
    return CBM_GOV_OK;
}

void cbm_gov_store_close(cbm_gov_store_t *store) {
    if (!store) {
        return;
    }
    if (store->db) {
        sqlite3_close(store->db);
        store->db = NULL;
    }
    free(store->db_path);
    free(store->project_id);
    free(store);
}

void cbm_gov_rule_free(cbm_tradition_rule_t *rule) {
    if (!rule) {
        return;
    }
    free(rule->rule_id);
    free(rule->scope_glob);
    free(rule->constraint_kind);
    free(rule->payload_json);
    free(rule->payload_hash);
    free(rule->status);
    free(rule->effective_status);
    free(rule->proposer_id);
    free(rule->origin_session_id);
    free(rule->evidence_refs_json);
    free(rule->reviewed_by);
    free(rule->superseded_by);
    memset(rule, 0, sizeof(*rule));
}

void cbm_gov_rules_free(cbm_tradition_rule_t *rules, int count) {
    if (!rules) {
        return;
    }
    for (int i = 0; i < count; i++) {
        cbm_gov_rule_free(&rules[i]);
    }
    free(rules);
}

void cbm_gov_manifest_free(cbm_admission_manifest_t *m) {
    if (!m) {
        return;
    }
    free(m->manifest_id);
    free(m->task_id);
    free(m->project_id);
    free(m->snapshot_id);
    free(m->graph_generation);
    free(m->rules_revision);
    free(m->coverage_revision);
    free(m->bundle_hash);
    free(m->fast_path_reasons_json);
    free(m->advisor_verdict_hash);
    free(m->blocking_violations_json);
    free(m->warnings_json);
    free(m->admission_status);
    memset(m, 0, sizeof(*m));
}

cbm_gov_status_t cbm_gov_get_state(cbm_gov_store_t *store,
                                   char *rules_revision_out, size_t rules_rev_sz,
                                   char *graph_gen_out, size_t graph_gen_sz,
                                   char *pub_status_out, size_t pub_status_sz,
                                   int64_t *last_trusted_time_out) {
    if (!store || !store->db) {
        return CBM_GOV_ERR_GENERIC;
    }
    const char *sql =
        "SELECT rules_revision, last_known_graph_generation, publication_status, last_trusted_time "
        "FROM governance_state WHERE singleton_id = 1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return CBM_GOV_ERR_GENERIC;
    }
    cbm_gov_status_t st = CBM_GOV_ERR_GENERIC;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *rr = (const char *)sqlite3_column_text(stmt, 0);
        const char *gg = (const char *)sqlite3_column_text(stmt, 1);
        const char *ps = (const char *)sqlite3_column_text(stmt, 2);
        int64_t ltt = sqlite3_column_int64(stmt, 3);
        if (rules_revision_out && rules_rev_sz > 0) {
            snprintf(rules_revision_out, rules_rev_sz, "%s", rr ? rr : "");
        }
        if (graph_gen_out && graph_gen_sz > 0) {
            snprintf(graph_gen_out, graph_gen_sz, "%s", gg ? gg : "");
        }
        if (pub_status_out && pub_status_sz > 0) {
            snprintf(pub_status_out, pub_status_sz, "%s", ps ? ps : "");
        }
        if (last_trusted_time_out) {
            *last_trusted_time_out = ltt;
        }
        st = CBM_GOV_OK;
    }
    sqlite3_finalize(stmt);
    return st;
}

cbm_gov_status_t cbm_gov_set_publication_status(cbm_gov_store_t *store,
                                                const char *publication_status,
                                                const char *graph_generation,
                                                int64_t now_epoch) {
    if (!store || !store->db || !publication_status) {
        return CBM_GOV_ERR_GENERIC;
    }
    if (strcmp(publication_status, "STABLE") != 0 &&
        strcmp(publication_status, "PUBLISHING") != 0 &&
        strcmp(publication_status, "RECOVERY_REQUIRED") != 0) {
        return CBM_GOV_ERR_INVALID_PAYLOAD;
    }
    if (now_epoch <= 0) {
        now_epoch = cbm_gov_now();
    }

    const char *sql = NULL;
    if (graph_generation && graph_generation[0]) {
        sql = "UPDATE governance_state SET publication_status = ?1, "
              "last_known_graph_generation = ?2, updated_at = ?3 WHERE singleton_id = 1;";
    } else {
        sql = "UPDATE governance_state SET publication_status = ?1, "
              "updated_at = ?2 WHERE singleton_id = 1;";
    }

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return CBM_GOV_ERR_GENERIC;
    }
    sqlite3_bind_text(stmt, 1, publication_status, -1, SQLITE_STATIC);
    if (graph_generation && graph_generation[0]) {
        sqlite3_bind_text(stmt, 2, graph_generation, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, now_epoch);
    } else {
        sqlite3_bind_int64(stmt, 2, now_epoch);
    }
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? CBM_GOV_OK : CBM_GOV_ERR_GENERIC;
}

static bool is_valid_constraint_kind(const char *k) {
    if (!k) {
        return false;
    }
    return strcmp(k, "FORBIDDEN_IMPORT") == 0 ||
           strcmp(k, "FORBIDDEN_CALL") == 0 ||
           strcmp(k, "REQUIRED_SEAM_CONTRACT") == 0 ||
           strcmp(k, "NAMING_CONVENTION") == 0 ||
           strcmp(k, "LAYER_BOUNDARY") == 0 ||
           strcmp(k, "EFFECT_RESTRICTION") == 0;
}

static bool check_interval_overflow(int64_t base_ts, int64_t interval_sec, int64_t *out_due) {
    if (interval_sec <= 0 || base_ts < 0) {
        return false;
    }
    if (base_ts > INT64_MAX - interval_sec) {
        return false;
    }
    if (out_due) {
        *out_due = base_ts + interval_sec;
    }
    return true;
}

/* Strip caller-injected privileged fields (status, reviewed_by, reviewed_at, review_due_at)
 * from payload_json and canonicalize it (U12, F05). */
static int sanitize_and_canonicalize_rule_payload(const char *raw_payload_json,
                                                  char **out_canonical,
                                                  char out_hash[65]) {
    if (!raw_payload_json || !raw_payload_json[0]) {
        return -1;
    }
    yyjson_doc *doc = yyjson_read(raw_payload_json, strlen(raw_payload_json), 0);
    if (!doc) {
        return -1;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return -1;
    }

    /* Rebuild object excluding privileged governance status/review keys */
    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
    yyjson_mut_doc_set_root(mdoc, mroot);

    yyjson_obj_iter iter;
    yyjson_obj_iter_init(root, &iter);
    yyjson_val *key = NULL;
    while ((key = yyjson_obj_iter_next(&iter)) != NULL) {
        const char *kstr = yyjson_get_str(key);
        yyjson_val *val = yyjson_obj_iter_get_val(key);
        if (!kstr) {
            continue;
        }
        if (strcmp(kstr, "status") == 0 ||
            strcmp(kstr, "reviewed_by") == 0 ||
            strcmp(kstr, "reviewed_at") == 0 ||
            strcmp(kstr, "review_due_at") == 0 ||
            strcmp(kstr, "effective_status") == 0) {
            continue;
        }
        yyjson_mut_val *mkey = yyjson_mut_strcpy(mdoc, kstr);
        yyjson_mut_val *mval = yyjson_val_mut_copy(mdoc, val);
        yyjson_mut_obj_add(mroot, mkey, mval);
    }

    size_t len = 0;
    char *tmp_json = yyjson_mut_write(mdoc, 0, &len);
    yyjson_mut_doc_free(mdoc);
    yyjson_doc_free(doc);
    if (!tmp_json) {
        return -1;
    }

    int rc = cbm_canonical_json(tmp_json, out_canonical);
    free(tmp_json);
    if (rc != 0 || !*out_canonical) {
        return -1;
    }
    cbm_sha256_hex(*out_canonical, strlen(*out_canonical), out_hash);
    return 0;
}

static void populate_rule_from_stmt(sqlite3_stmt *stmt, int64_t evaluation_time,
                                    cbm_tradition_rule_t *r) {
    memset(r, 0, sizeof(*r));
    r->rule_id = gov_strdup((const char *)sqlite3_column_text(stmt, 0));
    r->scope_glob = gov_strdup((const char *)sqlite3_column_text(stmt, 1));
    r->constraint_kind = gov_strdup((const char *)sqlite3_column_text(stmt, 2));
    r->payload_json = gov_strdup((const char *)sqlite3_column_text(stmt, 3));
    r->payload_hash = gov_strdup((const char *)sqlite3_column_text(stmt, 4));
    r->status = gov_strdup((const char *)sqlite3_column_text(stmt, 5));
    r->proposer_id = gov_strdup((const char *)sqlite3_column_text(stmt, 6));
    r->origin_session_id = gov_strdup((const char *)sqlite3_column_text(stmt, 7));
    r->evidence_refs_json = gov_strdup((const char *)sqlite3_column_text(stmt, 8));
    r->created_at = sqlite3_column_int64(stmt, 9);
    r->reviewed_at = sqlite3_column_int64(stmt, 10);
    r->reviewed_by = gov_strdup((const char *)sqlite3_column_text(stmt, 11));
    r->review_interval_seconds = sqlite3_column_int64(stmt, 12);
    r->review_due_at = sqlite3_column_int64(stmt, 13);
    r->superseded_by = gov_strdup((const char *)sqlite3_column_text(stmt, 14));

    if (r->status && strcmp(r->status, "APPROVED") == 0 &&
        r->review_due_at > 0 && evaluation_time >= r->review_due_at) {
        r->effective_status = gov_strdup("REVIEW_PENDING");
    } else {
        r->effective_status = gov_strdup(r->status ? r->status : "UNVERIFIED_CANDIDATE");
    }
}

cbm_gov_status_t cbm_gov_get_rule(cbm_gov_store_t *store,
                                  const char *rule_id,
                                  int64_t evaluation_time,
                                  cbm_tradition_rule_t *out_rule) {
    if (!store || !store->db || !rule_id || !out_rule) {
        return CBM_GOV_ERR_GENERIC;
    }
    if (evaluation_time <= 0) {
        evaluation_time = (int64_t)time(NULL);
    }
    const char *sql =
        "SELECT rule_id, scope_glob, constraint_kind, payload_json, payload_hash, "
        "status, proposer_id, origin_session_id, evidence_refs_json, created_at, "
        "reviewed_at, reviewed_by, review_interval_seconds, review_due_at, superseded_by "
        "FROM tradition_rules WHERE rule_id = ?1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return CBM_GOV_ERR_GENERIC;
    }
    sqlite3_bind_text(stmt, 1, rule_id, -1, SQLITE_STATIC);
    cbm_gov_status_t st = CBM_GOV_ERR_RULE_NOT_FOUND;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        populate_rule_from_stmt(stmt, evaluation_time, out_rule);
        st = CBM_GOV_OK;
    }
    sqlite3_finalize(stmt);
    return st;
}

cbm_gov_status_t cbm_gov_evaluate_active_rules(cbm_gov_store_t *store,
                                               int64_t evaluation_time,
                                               cbm_tradition_rule_t **out_rules,
                                               int *out_count,
                                               char rules_revision_out[65],
                                               char **out_active_rules_json) {
    if (!store || !store->db) {
        return CBM_GOV_ERR_GENERIC;
    }
    if (out_rules) {
        *out_rules = NULL;
    }
    if (out_count) {
        *out_count = 0;
    }
    if (out_active_rules_json) {
        *out_active_rules_json = NULL;
    }
    if (evaluation_time <= 0) {
        evaluation_time = (int64_t)time(NULL);
    }

    int64_t last_trusted_time = 0;
    if (cbm_gov_get_state(store, NULL, 0, NULL, 0, NULL, 0, &last_trusted_time) != CBM_GOV_OK) {
        return CBM_GOV_ERR_GENERIC;
    }
    /* Monotonic clock check: if evaluation_time < last_trusted_time, fail closed (U11, §5.2) */
    if (evaluation_time < last_trusted_time) {
        gov_set_error(store, "Evaluation time precedes last_trusted_time (CLOCK_UNCERTAIN)");
        return CBM_GOV_ERR_CLOCK_UNCERTAIN;
    }

    const char *sql =
        "SELECT rule_id, scope_glob, constraint_kind, payload_json, payload_hash, "
        "status, proposer_id, origin_session_id, evidence_refs_json, created_at, "
        "reviewed_at, reviewed_by, review_interval_seconds, review_due_at, superseded_by "
        "FROM tradition_rules WHERE status = 'APPROVED' ORDER BY rule_id ASC;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return CBM_GOV_ERR_GENERIC;
    }

    int cap = 8;
    int count = 0;
    cbm_tradition_rule_t *rules = (cbm_tradition_rule_t *)calloc((size_t)cap, sizeof(cbm_tradition_rule_t));
    if (!rules) {
        sqlite3_finalize(stmt);
        return CBM_GOV_ERR_GENERIC;
    }

    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *marr = yyjson_mut_arr(mdoc);
    yyjson_mut_doc_set_root(mdoc, marr);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            cap *= 2;
            cbm_tradition_rule_t *nrules = (cbm_tradition_rule_t *)realloc(
                rules, (size_t)cap * sizeof(cbm_tradition_rule_t));
            if (!nrules) {
                cbm_gov_rules_free(rules, count);
                yyjson_mut_doc_free(mdoc);
                sqlite3_finalize(stmt);
                return CBM_GOV_ERR_GENERIC;
            }
            rules = nrules;
        }
        populate_rule_from_stmt(stmt, evaluation_time, &rules[count]);
        cbm_tradition_rule_t *r = &rules[count];

        yyjson_mut_val *robj = yyjson_mut_obj(mdoc);
        yyjson_mut_obj_add_strcpy(mdoc, robj, "constraint_kind", r->constraint_kind ? r->constraint_kind : "");
        yyjson_mut_obj_add_strcpy(mdoc, robj, "effective_status", r->effective_status ? r->effective_status : "APPROVED");
        yyjson_mut_obj_add_strcpy(mdoc, robj, "payload_hash", r->payload_hash ? r->payload_hash : "");
        if (r->payload_json) {
            yyjson_doc *pdoc = yyjson_read(r->payload_json, strlen(r->payload_json), 0);
            if (pdoc && yyjson_doc_get_root(pdoc)) {
                yyjson_mut_obj_add_val(mdoc, robj, "payload",
                                       yyjson_val_mut_copy(mdoc, yyjson_doc_get_root(pdoc)));
            }
            if (pdoc) {
                yyjson_doc_free(pdoc);
            }
        }
        yyjson_mut_obj_add_int(mdoc, robj, "review_due_at", r->review_due_at);
        yyjson_mut_obj_add_strcpy(mdoc, robj, "rule_id", r->rule_id ? r->rule_id : "");
        yyjson_mut_obj_add_strcpy(mdoc, robj, "scope_glob", r->scope_glob ? r->scope_glob : "");
        yyjson_mut_obj_add_strcpy(mdoc, robj, "status", r->status ? r->status : "APPROVED");
        yyjson_mut_arr_append(marr, robj);

        count++;
    }
    sqlite3_finalize(stmt);

    size_t raw_len = 0;
    char *raw_json = yyjson_mut_write(mdoc, 0, &raw_len);
    yyjson_mut_doc_free(mdoc);
    if (!raw_json) {
        cbm_gov_rules_free(rules, count);
        return CBM_GOV_ERR_GENERIC;
    }

    char *canonical_rules = NULL;
    if (cbm_canonical_json(raw_json, &canonical_rules) != 0 || !canonical_rules) {
        free(raw_json);
        cbm_gov_rules_free(rules, count);
        return CBM_GOV_ERR_GENERIC;
    }
    free(raw_json);

    char rev[65];
    cbm_sha256_hex(canonical_rules, strlen(canonical_rules), rev);
    if (rules_revision_out) {
        memcpy(rules_revision_out, rev, 65);
    }

    if (out_active_rules_json) {
        *out_active_rules_json = canonical_rules;
    } else {
        free(canonical_rules);
    }

    if (out_rules) {
        *out_rules = rules;
    } else {
        cbm_gov_rules_free(rules, count);
    }
    if (out_count) {
        *out_count = count;
    }
    return CBM_GOV_OK;
}

static int count_json_array_items(const char *json_str);

cbm_gov_status_t cbm_gov_propose_rule(cbm_gov_store_t *store,
                                      const char *rule_id,
                                      const char *scope_glob,
                                      const char *constraint_kind,
                                      const char *payload_json,
                                      const char *proposer_id,
                                      const char *origin_session_id,
                                      const char *evidence_refs_json,
                                      int64_t review_interval_seconds,
                                      int64_t now_epoch,
                                      bool *out_idempotent_replay) {
    if (out_idempotent_replay) {
        *out_idempotent_replay = false;
    }
    if (!store || !store->db || !rule_id || !rule_id[0] || !scope_glob || !scope_glob[0] ||
        !is_valid_constraint_kind(constraint_kind)) {
        gov_set_error(store, "Invalid rule_id, scope_glob, or constraint_kind");
        return CBM_GOV_ERR_INVALID_PAYLOAD;
    }
    if (now_epoch <= 0) {
        now_epoch = cbm_gov_now();
    }
    if (!check_interval_overflow(now_epoch, review_interval_seconds, NULL)) {
        gov_set_error(store, "Invalid or overflowing review_interval_seconds");
        return CBM_GOV_ERR_INVALID_INTERVAL;
    }

    char *canonical_payload = NULL;
    char payload_hash[65];
    if (sanitize_and_canonicalize_rule_payload(payload_json, &canonical_payload, payload_hash) != 0) {
        gov_set_error(store, "Malformed or non-object payload_json");
        return CBM_GOV_ERR_INVALID_PAYLOAD;
    }

    const char *safe_proposer = (proposer_id && proposer_id[0]) ? proposer_id : "agent";
    const char *safe_session = (origin_session_id && origin_session_id[0]) ? origin_session_id : "session-default";
    const char *safe_evidence = (evidence_refs_json && evidence_refs_json[0]) ? evidence_refs_json : "[]";
    if (count_json_array_items(safe_evidence) < 0) {
        free(canonical_payload);
        gov_set_error(store, "evidence_refs_json must be a valid JSON array");
        return CBM_GOV_ERR_INVALID_PAYLOAD;
    }

    /* Check if rule_id already exists (Scenario I18):
     * Identical payload/scope/kind is an idempotent no-op even if subsequently approved (003 §3.3). */
    cbm_tradition_rule_t existing = {0};
    cbm_gov_status_t get_st = cbm_gov_get_rule(store, rule_id, now_epoch, &existing);
    if (get_st == CBM_GOV_OK) {
        bool same = (strcmp(existing.payload_hash, payload_hash) == 0 &&
                     strcmp(existing.scope_glob, scope_glob) == 0 &&
                     strcmp(existing.constraint_kind, constraint_kind) == 0 &&
                     existing.review_interval_seconds == review_interval_seconds);
        cbm_gov_rule_free(&existing);
        free(canonical_payload);
        if (same) {
            if (out_idempotent_replay) {
                *out_idempotent_replay = true;
            }
            return CBM_GOV_OK;
        }
        gov_set_error(store, "Rule already exists with different payload or parameters (RULE_CONFLICT)");
        return CBM_GOV_ERR_RULE_CONFLICT;
    }

    const char *ins_sql =
        "INSERT INTO tradition_rules "
        "(rule_id, scope_glob, constraint_kind, payload_json, payload_hash, status, "
        " proposer_id, origin_session_id, evidence_refs_json, created_at, reviewed_at, "
        " reviewed_by, review_interval_seconds, review_due_at, superseded_by) "
        "VALUES (?1, ?2, ?3, ?4, ?5, 'UNVERIFIED_CANDIDATE', ?6, ?7, ?8, ?9, 0, NULL, ?10, 0, NULL);";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, ins_sql, -1, &stmt, NULL) != SQLITE_OK) {
        free(canonical_payload);
        return CBM_GOV_ERR_GENERIC;
    }
    sqlite3_bind_text(stmt, 1, rule_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, scope_glob, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, constraint_kind, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, canonical_payload, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, payload_hash, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, safe_proposer, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, safe_session, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, safe_evidence, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 9, now_epoch);
    sqlite3_bind_int64(stmt, 10, review_interval_seconds);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    free(canonical_payload);
    if (rc != SQLITE_DONE) {
        gov_set_error(store, sqlite3_errmsg(store->db));
        return CBM_GOV_ERR_GENERIC;
    }
    return CBM_GOV_OK;
}

cbm_gov_status_t cbm_gov_review_rule(cbm_gov_store_t *store,
                                     const char *rule_id,
                                     const char *new_status,
                                     const char *reviewer_id,
                                     bool operator_verified,
                                     const char *rationale,
                                     int64_t review_interval_seconds,
                                     const char *superseded_by,
                                     int64_t now_epoch) {
    if (!store || !store->db || !rule_id || !new_status) {
        return CBM_GOV_ERR_INVALID_PAYLOAD;
    }
    /* Security check (Scenarios U13, U14, I17): only verified host-authenticated operator */
    if (!operator_verified || !reviewer_id || !reviewer_id[0]) {
        gov_set_error(store, "Operator authority not verified for tradition review (UNAUTHORIZED_REVIEWER)");
        return CBM_GOV_ERR_UNAUTHORIZED_REVIEWER;
    }
    if (strcmp(new_status, "APPROVED") != 0 &&
        strcmp(new_status, "REJECTED") != 0 &&
        strcmp(new_status, "SUPERSEDED") != 0) {
        gov_set_error(store, "Invalid review target status");
        return CBM_GOV_ERR_INVALID_PAYLOAD;
    }
    if (now_epoch <= 0) {
        now_epoch = cbm_gov_now();
    }

    char *errmsg = NULL;
    if (sqlite3_exec(store->db, "BEGIN IMMEDIATE;", NULL, NULL, &errmsg) != SQLITE_OK) {
        sqlite3_free(errmsg);
        return CBM_GOV_ERR_GENERIC;
    }

    int64_t last_trusted_time = 0;
    if (cbm_gov_get_state(store, NULL, 0, NULL, 0, NULL, 0, &last_trusted_time) != CBM_GOV_OK) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        return CBM_GOV_ERR_GENERIC;
    }
    if (now_epoch < last_trusted_time) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        gov_set_error(store, "Review timestamp precedes last_trusted_time (CLOCK_UNCERTAIN)");
        return CBM_GOV_ERR_CLOCK_UNCERTAIN;
    }

    cbm_tradition_rule_t existing = {0};
    if (cbm_gov_get_rule(store, rule_id, now_epoch, &existing) != CBM_GOV_OK) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        gov_set_error(store, "Rule not found for review");
        return CBM_GOV_ERR_RULE_NOT_FOUND;
    }

    int64_t interval = (review_interval_seconds > 0)
                           ? review_interval_seconds
                           : existing.review_interval_seconds;
    int64_t due_at = 0;
    if (!check_interval_overflow(now_epoch, interval, &due_at)) {
        cbm_gov_rule_free(&existing);
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        gov_set_error(store, "Invalid or overflowing review_interval_seconds");
        return CBM_GOV_ERR_INVALID_INTERVAL;
    }

    const char *upd_sql =
        "UPDATE tradition_rules SET status = ?1, reviewed_at = ?2, reviewed_by = ?3, "
        "review_interval_seconds = ?4, review_due_at = ?5, superseded_by = ?6 "
        "WHERE rule_id = ?7;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, upd_sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        cbm_gov_rule_free(&existing);
        return CBM_GOV_ERR_GENERIC;
    }
    sqlite3_bind_text(stmt, 1, new_status, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, now_epoch);
    sqlite3_bind_text(stmt, 3, reviewer_id, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, interval);
    sqlite3_bind_int64(stmt, 5, (strcmp(new_status, "APPROVED") == 0) ? due_at : 0);
    if (superseded_by && superseded_by[0]) {
        sqlite3_bind_text(stmt, 6, superseded_by, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 6);
    }
    sqlite3_bind_text(stmt, 7, rule_id, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        cbm_gov_rule_free(&existing);
        return CBM_GOV_ERR_GENERIC;
    }
    sqlite3_finalize(stmt);

    /* Append audit row to tradition_reviews */
    const char *rev_sql =
        "INSERT INTO tradition_reviews "
        "(rule_id, previous_status, new_status, reviewer_id, rationale, reviewed_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6);";
    if (sqlite3_prepare_v2(store->db, rev_sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        cbm_gov_rule_free(&existing);
        return CBM_GOV_ERR_GENERIC;
    }
    sqlite3_bind_text(stmt, 1, rule_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, existing.status ? existing.status : "UNVERIFIED_CANDIDATE", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, new_status, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, reviewer_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, (rationale && rationale[0]) ? rationale : "Reviewed by operator", -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, now_epoch);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        cbm_gov_rule_free(&existing);
        return CBM_GOV_ERR_GENERIC;
    }
    sqlite3_finalize(stmt);
    cbm_gov_rule_free(&existing);

    /* Recompute rules_revision atomically INSIDE the transaction before COMMIT */
    char new_rev[65] = {0};
    if (cbm_gov_evaluate_active_rules(store, now_epoch, NULL, NULL, new_rev, NULL) != CBM_GOV_OK) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        return CBM_GOV_ERR_GENERIC;
    }

    const char *st_sql =
        "UPDATE governance_state SET rules_revision = ?1, last_trusted_time = ?2, updated_at = ?2 WHERE singleton_id = 1;";
    if (sqlite3_prepare_v2(store->db, st_sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        return CBM_GOV_ERR_GENERIC;
    }
    sqlite3_bind_text(stmt, 1, new_rev, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, now_epoch);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        return CBM_GOV_ERR_GENERIC;
    }
    sqlite3_finalize(stmt);

    if (sqlite3_exec(store->db, "COMMIT;", NULL, NULL, &errmsg) != SQLITE_OK) {
        sqlite3_free(errmsg);
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        return CBM_GOV_ERR_GENERIC;
    }
    return CBM_GOV_OK;
}

static int count_json_array_items(const char *json_str) {
    if (!json_str || !json_str[0]) {
        return 0;
    }
    yyjson_doc *doc = yyjson_read(json_str, strlen(json_str), 0);
    if (!doc) {
        return -1;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_arr(root)) {
        yyjson_doc_free(doc);
        return -1;
    }
    int count = (int)yyjson_arr_size(root);
    yyjson_doc_free(doc);
    return count;
}

cbm_gov_status_t cbm_gov_get_manifest(cbm_gov_store_t *store,
                                      const char *manifest_id,
                                      cbm_admission_manifest_t *out_manifest) {
    if (!store || !store->db || !manifest_id || !out_manifest) {
        return CBM_GOV_ERR_GENERIC;
    }
    memset(out_manifest, 0, sizeof(*out_manifest));
    const char *sql =
        "SELECT manifest_id, task_id, project_id, snapshot_id, graph_generation, "
        "rules_revision, coverage_revision, bundle_hash, fast_path_granted, "
        "fast_path_reasons_json, advisor_invoked, advisor_verdict_hash, "
        "blocking_violations_json, warnings_json, tdd_oracles_passed, "
        "admission_status, created_at "
        "FROM harness_admission_manifests WHERE manifest_id = ?1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return CBM_GOV_ERR_GENERIC;
    }
    sqlite3_bind_text(stmt, 1, manifest_id, -1, SQLITE_STATIC);
    cbm_gov_status_t st = CBM_GOV_ERR_GENERIC;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_manifest->manifest_id = gov_strdup((const char *)sqlite3_column_text(stmt, 0));
        out_manifest->task_id = gov_strdup((const char *)sqlite3_column_text(stmt, 1));
        out_manifest->project_id = gov_strdup((const char *)sqlite3_column_text(stmt, 2));
        out_manifest->snapshot_id = gov_strdup((const char *)sqlite3_column_text(stmt, 3));
        out_manifest->graph_generation = gov_strdup((const char *)sqlite3_column_text(stmt, 4));
        out_manifest->rules_revision = gov_strdup((const char *)sqlite3_column_text(stmt, 5));
        out_manifest->coverage_revision = gov_strdup((const char *)sqlite3_column_text(stmt, 6));
        out_manifest->bundle_hash = gov_strdup((const char *)sqlite3_column_text(stmt, 7));
        out_manifest->fast_path_granted = sqlite3_column_int(stmt, 8);
        out_manifest->fast_path_reasons_json = gov_strdup((const char *)sqlite3_column_text(stmt, 9));
        out_manifest->advisor_invoked = sqlite3_column_int(stmt, 10);
        out_manifest->advisor_verdict_hash = gov_strdup((const char *)sqlite3_column_text(stmt, 11));
        out_manifest->blocking_violations_json = gov_strdup((const char *)sqlite3_column_text(stmt, 12));
        out_manifest->warnings_json = gov_strdup((const char *)sqlite3_column_text(stmt, 13));
        out_manifest->tdd_oracles_passed = sqlite3_column_int(stmt, 14);
        out_manifest->admission_status = gov_strdup((const char *)sqlite3_column_text(stmt, 15));
        out_manifest->created_at = sqlite3_column_int64(stmt, 16);
        st = CBM_GOV_OK;
    }
    sqlite3_finalize(stmt);
    return st;
}

cbm_gov_status_t cbm_gov_record_manifest(cbm_gov_store_t *store,
                                         const cbm_admission_manifest_t *manifest,
                                         const char *expected_snapshot_id,
                                         const char *expected_graph_generation,
                                         const char *expected_rules_revision,
                                         const char *expected_coverage_revision,
                                         bool server_fast_path_eligible,
                                         int64_t now_epoch,
                                         char admission_status_out[32],
                                         bool *out_idempotent_replay) {
    if (out_idempotent_replay) {
        *out_idempotent_replay = false;
    }
    if (admission_status_out) {
        admission_status_out[0] = '\0';
    }
    if (!store || !store->db || !manifest || !manifest->manifest_id || !manifest->manifest_id[0] ||
        !manifest->task_id || !manifest->task_id[0] ||
        !manifest->snapshot_id || !cbm_harness_validate_sha256_hex(manifest->snapshot_id) ||
        !manifest->bundle_hash || !cbm_harness_validate_sha256_hex(manifest->bundle_hash)) {
        gov_set_error(store, "Missing or invalid required manifest fields (64-char hex snapshot_id and bundle_hash required)");
        return CBM_GOV_ERR_INVALID_PAYLOAD;
    }

    int blocking_count = count_json_array_items(manifest->blocking_violations_json);
    int warning_count = count_json_array_items(manifest->warnings_json);
    int reasons_count = count_json_array_items(manifest->fast_path_reasons_json);
    if (blocking_count < 0 || warning_count < 0 || reasons_count < 0) {
        gov_set_error(store, "blocking_violations_json, warnings_json, and fast_path_reasons_json must be valid JSON arrays");
        return CBM_GOV_ERR_INVALID_PAYLOAD;
    }

    if (now_epoch <= 0) {
        now_epoch = cbm_gov_now();
    }

    char *errmsg = NULL;
    if (sqlite3_exec(store->db, "BEGIN IMMEDIATE;", NULL, NULL, &errmsg) != SQLITE_OK) {
        sqlite3_free(errmsg);
        return CBM_GOV_ERR_GENERIC;
    }

    /* 1. Check publication fence and clock certainty INSIDE BEGIN IMMEDIATE (Scenarios I11, I13) */
    char pub_status[32] = {0};
    int64_t last_trusted_time = 0;
    if (cbm_gov_get_state(store, NULL, 0, NULL, 0, pub_status, sizeof(pub_status), &last_trusted_time) != CBM_GOV_OK) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        return CBM_GOV_ERR_GENERIC;
    }
    if (strcmp(pub_status, "STABLE") != 0) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        gov_set_error(store, "Graph publication fence active (PUBLICATION_IN_PROGRESS)");
        return CBM_GOV_ERR_PUBLICATION_IN_PROGRESS;
    }
    if (now_epoch < last_trusted_time) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        gov_set_error(store, "Evaluation time precedes last_trusted_time (CLOCK_UNCERTAIN)");
        return CBM_GOV_ERR_CLOCK_UNCERTAIN;
    }

    /* Recompute live rules_revision inside the transaction to prevent TOCTOU (Codex P1 #4) */
    char tx_rules_rev[65] = {0};
    cbm_gov_status_t rev_st = cbm_gov_evaluate_active_rules(store, now_epoch, NULL, NULL, tx_rules_rev, NULL);
    if (rev_st != CBM_GOV_OK) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        return rev_st;
    }
    const char *effective_expected_rules_rev =
        (expected_rules_revision && expected_rules_revision[0]) ? expected_rules_revision : tx_rules_rev;
    if (strcmp(tx_rules_rev, effective_expected_rules_rev) != 0) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        gov_set_error(store, "Concurrent tradition rule change or cadence expiry detected inside transaction (STALE_SNAPSHOT)");
        return CBM_GOV_ERR_STALE_SNAPSHOT;
    }

    const char *proj_id = (manifest->project_id && manifest->project_id[0])
                              ? manifest->project_id
                              : store->project_id;
    int effective_fast_path = (blocking_count > 0) ? 0 : (manifest->fast_path_granted ? 1 : 0);

    /* 2. Check idempotent replay or conflict on manifest_id across ALL immutable fields (Scenario I15) */
    cbm_admission_manifest_t existing = {0};
    if (cbm_gov_get_manifest(store, manifest->manifest_id, &existing) == CBM_GOV_OK) {
        const char *ex_adv = existing.advisor_verdict_hash ? existing.advisor_verdict_hash : "";
        const char *in_adv = manifest->advisor_verdict_hash ? manifest->advisor_verdict_hash : "";
        const char *ex_gg = existing.graph_generation ? existing.graph_generation : "";
        const char *in_gg = manifest->graph_generation ? manifest->graph_generation : "";
        const char *ex_rr = existing.rules_revision ? existing.rules_revision : "";
        const char *in_rr = manifest->rules_revision ? manifest->rules_revision : "";
        const char *ex_cr = existing.coverage_revision ? existing.coverage_revision : "";
        const char *in_cr = manifest->coverage_revision ? manifest->coverage_revision : "";

        bool identical = (strcmp(existing.task_id, manifest->task_id) == 0 &&
                          strcmp(existing.project_id, proj_id) == 0 &&
                          strcmp(existing.snapshot_id, manifest->snapshot_id) == 0 &&
                          strcmp(ex_gg, in_gg) == 0 &&
                          strcmp(ex_rr, in_rr) == 0 &&
                          strcmp(ex_cr, in_cr) == 0 &&
                          strcmp(existing.bundle_hash, manifest->bundle_hash) == 0 &&
                          existing.fast_path_granted == effective_fast_path &&
                          strcmp(existing.fast_path_reasons_json, manifest->fast_path_reasons_json) == 0 &&
                          existing.advisor_invoked == (manifest->advisor_invoked ? 1 : 0) &&
                          strcmp(ex_adv, in_adv) == 0 &&
                          strcmp(existing.blocking_violations_json, manifest->blocking_violations_json) == 0 &&
                          strcmp(existing.warnings_json, manifest->warnings_json) == 0 &&
                          existing.tdd_oracles_passed == (manifest->tdd_oracles_passed ? 1 : 0));
        char saved_adm_status[32] = {0};
        snprintf(saved_adm_status, sizeof(saved_adm_status), "%s",
                 existing.admission_status ? existing.admission_status : "ADMITTED_CLEAN");
        cbm_gov_manifest_free(&existing);
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        if (!identical) {
            gov_set_error(store, "manifest_id already exists with different payload (MANIFEST_ID_CONFLICT)");
            return CBM_GOV_ERR_MANIFEST_ID_CONFLICT;
        }
        if (out_idempotent_replay) {
            *out_idempotent_replay = true;
        }
        if (admission_status_out) {
            snprintf(admission_status_out, 32, "%s", saved_adm_status);
        }
        if (strcmp(saved_adm_status, "REJECTED_BLOCKING") == 0) {
            return CBM_GOV_ERR_BLOCKING_VIOLATIONS;
        }
        return CBM_GOV_OK;
    }

    /* 3. Verify composite snapshot_id against live transaction state (Scenarios I10, I12) */
    if ((expected_snapshot_id && expected_snapshot_id[0] &&
         strcmp(manifest->snapshot_id, expected_snapshot_id) != 0) ||
        (expected_graph_generation && expected_graph_generation[0] &&
         manifest->graph_generation &&
         strcmp(manifest->graph_generation, expected_graph_generation) != 0) ||
        (manifest->rules_revision && manifest->rules_revision[0] &&
         strcmp(manifest->rules_revision, tx_rules_rev) != 0) ||
        (expected_coverage_revision && expected_coverage_revision[0] &&
         manifest->coverage_revision &&
         strcmp(manifest->coverage_revision, expected_coverage_revision) != 0)) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        gov_set_error(store, "Snapshot mismatch: graph_generation, rules_revision, or coverage_revision changed (STALE_SNAPSHOT)");
        return CBM_GOV_ERR_STALE_SNAPSHOT;
    }

    /* 4. If no blocking violations, verify Fast-Path claim against server-side eligibility (Scenario F03) */
    if (blocking_count == 0 && manifest->fast_path_granted == 1 && !server_fast_path_eligible) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        gov_set_error(store, "Caller claimed fast_path_granted=1 on ineligible bundle (FAST_PATH_NOT_ELIGIBLE)");
        return CBM_GOV_ERR_FAST_PATH_NOT_ELIGIBLE;
    }

    /* 5. Option A (Full Forensic Trail) status & fast_path resolution (Scenarios U17, I14, I16):
     * - blocking_count > 0 -> REJECTED_BLOCKING, fast_path_granted forced to 0
     * - blocking_count == 0 && warning_count > 0 -> ADMITTED_WITH_WARNINGS
     * - blocking_count == 0 && warning_count == 0 -> ADMITTED_CLEAN */
    const char *adm_status = "ADMITTED_CLEAN";
    if (blocking_count > 0) {
        adm_status = "REJECTED_BLOCKING";
        effective_fast_path = 0;
    } else if (warning_count > 0) {
        adm_status = "ADMITTED_WITH_WARNINGS";
    }
    if (admission_status_out) {
        snprintf(admission_status_out, 32, "%s", adm_status);
    }

    const char *ins_sql =
        "INSERT INTO harness_admission_manifests "
        "(manifest_id, task_id, project_id, snapshot_id, graph_generation, "
        " rules_revision, coverage_revision, bundle_hash, fast_path_granted, "
        " fast_path_reasons_json, advisor_invoked, advisor_verdict_hash, "
        " blocking_violations_json, warnings_json, tdd_oracles_passed, "
        " admission_status, created_at) "
        "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17);";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->db, ins_sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        return CBM_GOV_ERR_GENERIC;
    }

    sqlite3_bind_text(stmt, 1, manifest->manifest_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, manifest->task_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, proj_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, manifest->snapshot_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, manifest->graph_generation ? manifest->graph_generation : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, manifest->rules_revision ? manifest->rules_revision : tx_rules_rev, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, manifest->coverage_revision ? manifest->coverage_revision : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, manifest->bundle_hash, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 9, effective_fast_path);
    sqlite3_bind_text(stmt, 10, manifest->fast_path_reasons_json, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 11, manifest->advisor_invoked ? 1 : 0);
    if (manifest->advisor_verdict_hash && manifest->advisor_verdict_hash[0]) {
        sqlite3_bind_text(stmt, 12, manifest->advisor_verdict_hash, -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 12);
    }
    sqlite3_bind_text(stmt, 13, manifest->blocking_violations_json, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 14, manifest->warnings_json, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 15, manifest->tdd_oracles_passed ? 1 : 0);
    sqlite3_bind_text(stmt, 16, adm_status, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 17, now_epoch);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        gov_set_error(store, sqlite3_errmsg(store->db));
        return CBM_GOV_ERR_GENERIC;
    }

    if (sqlite3_exec(store->db, "COMMIT;", NULL, NULL, &errmsg) != SQLITE_OK) {
        sqlite3_free(errmsg);
        sqlite3_exec(store->db, "ROLLBACK;", NULL, NULL, NULL);
        return CBM_GOV_ERR_GENERIC;
    }

    if (blocking_count > 0) {
        gov_set_error(store, "Admission rejected due to blocking violations (recorded as REJECTED_BLOCKING)");
        return CBM_GOV_ERR_BLOCKING_VIOLATIONS;
    }
    return CBM_GOV_OK;
}
