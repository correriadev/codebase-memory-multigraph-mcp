#ifndef CBM_GOVERNANCE_STORE_H
#define CBM_GOVERNANCE_STORE_H

#include <stdbool.h>
#include <stdint.h>
#include "sqlite3.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CBM_GOV_OK = 0,
    CBM_GOV_ERR_GENERIC = -1,
    CBM_GOV_ERR_PRAGMA_MISMATCH = -2,
    CBM_GOV_ERR_UNAUTHORIZED_REVIEWER = -3,
    CBM_GOV_ERR_RULE_CONFLICT = -4,
    CBM_GOV_ERR_RULE_NOT_FOUND = -5,
    CBM_GOV_ERR_CLOCK_UNCERTAIN = -6,
    CBM_GOV_ERR_INVALID_PAYLOAD = -7,
    CBM_GOV_ERR_INVALID_INTERVAL = -8,
    CBM_GOV_ERR_PUBLICATION_IN_PROGRESS = -9,
    CBM_GOV_ERR_STALE_SNAPSHOT = -10,
    CBM_GOV_ERR_MANIFEST_ID_CONFLICT = -11,
    CBM_GOV_ERR_FAST_PATH_NOT_ELIGIBLE = -12,
    CBM_GOV_ERR_BLOCKING_VIOLATIONS = -13
} cbm_gov_status_t;

typedef struct cbm_gov_store cbm_gov_store_t;

typedef struct {
    char *rule_id;
    char *scope_glob;
    char *constraint_kind;
    char *payload_json;
    char *payload_hash;
    char *status;           /* DB status: UNVERIFIED_CANDIDATE, APPROVED, REJECTED, SUPERSEDED */
    char *effective_status; /* Projected status at evaluation_time: may be REVIEW_PENDING */
    char *proposer_id;
    char *origin_session_id;
    char *evidence_refs_json;
    int64_t created_at;
    int64_t reviewed_at;
    char *reviewed_by;
    int64_t review_interval_seconds;
    int64_t review_due_at;
    char *superseded_by;
} cbm_tradition_rule_t;

typedef struct {
    char *manifest_id;
    char *task_id;
    char *project_id;
    char *snapshot_id;
    char *graph_generation;
    char *rules_revision;
    char *coverage_revision;
    char *bundle_hash;
    int fast_path_granted;
    char *fast_path_reasons_json;
    int advisor_invoked;
    char *advisor_verdict_hash;
    char *blocking_violations_json;
    char *warnings_json;
    int tdd_oracles_passed;
    char *admission_status; /* ADMITTED_CLEAN, ADMITTED_WITH_WARNINGS, REJECTED_BLOCKING */
    int64_t created_at;
} cbm_admission_manifest_t;

/* Resolve default governance DB path for a project (<cache_dir>/<project>.governance.db).
 * Caller must free() the returned string. */
char *cbm_gov_resolve_db_path(const char *project_id, const char *custom_dir);

/* Open or create the governance SQLite WAL store for a project.
 * Enforces and verifies WAL + FULL synchronous + foreign_keys. */
cbm_gov_status_t cbm_gov_store_open(const char *db_path, const char *project_id,
                                    cbm_gov_store_t **out_store);

/* Close governance store and release SQLite handle. */
void cbm_gov_store_close(cbm_gov_store_t *store);

/* Return raw sqlite3 handle for white-box testing / inspection. */
sqlite3 *cbm_gov_store_get_db(cbm_gov_store_t *store);

/* Return last error message recorded on the store. */
const char *cbm_gov_store_last_error(const cbm_gov_store_t *store);

/* Return canonical error code string (e.g. "STALE_SNAPSHOT", "BLOCKING_VIOLATIONS"). */
const char *cbm_gov_status_code_str(cbm_gov_status_t status);

/* Set publication_status ('PUBLISHING' or 'STABLE') and optionally update last_known_graph_generation. */
cbm_gov_status_t cbm_gov_set_publication_status(cbm_gov_store_t *store,
                                                const char *publication_status,
                                                const char *graph_generation,
                                                int64_t now_epoch);

/* Get current governance_state fields. Buffers must be at least 65 bytes for revisions. */
cbm_gov_status_t cbm_gov_get_state(cbm_gov_store_t *store,
                                   char *rules_revision_out, size_t rules_rev_sz,
                                   char *graph_gen_out, size_t graph_gen_sz,
                                   char *pub_status_out, size_t pub_status_sz,
                                   int64_t *last_trusted_time_out);

/* Propose a new tradition rule. Always enters as UNVERIFIED_CANDIDATE with reviewed_at=0.
 * Idempotent on identical (rule_id, payload_hash, scope_glob, constraint_kind). */
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
                                      bool *out_idempotent_replay);

/* Review a tradition rule. Requires verified host-authenticated operator capability. */
cbm_gov_status_t cbm_gov_review_rule(cbm_gov_store_t *store,
                                     const char *rule_id,
                                     const char *new_status,
                                     const char *reviewer_id,
                                     bool operator_verified,
                                     const char *rationale,
                                     int64_t review_interval_seconds,
                                     const char *superseded_by,
                                     int64_t now_epoch);

/* Query active rules (APPROVED, projected to REVIEW_PENDING if now_epoch >= review_due_at).
 * Also computes deterministic rules_revision (64-char hex + NUL) at evaluation_time.
 * If now_epoch < last_trusted_time, returns CBM_GOV_ERR_CLOCK_UNCERTAIN. */
cbm_gov_status_t cbm_gov_evaluate_active_rules(cbm_gov_store_t *store,
                                               int64_t evaluation_time,
                                               cbm_tradition_rule_t **out_rules,
                                               int *out_count,
                                               char rules_revision_out[65],
                                               char **out_active_rules_json);

/* Lookup a single rule by rule_id (any status). */
cbm_gov_status_t cbm_gov_get_rule(cbm_gov_store_t *store,
                                  const char *rule_id,
                                  int64_t evaluation_time,
                                  cbm_tradition_rule_t *out_rule);

void cbm_gov_rule_free(cbm_tradition_rule_t *rule);
void cbm_gov_rules_free(cbm_tradition_rule_t *rules, int count);

/* Record an admission manifest under BEGIN IMMEDIATE.
 * Enforces Option A (Full Forensic Trail):
 * - Rejects stale snapshot, publication in progress, manifest_id conflict, or ineligible fast_path.
 * - If blocking_violations_json is non-empty: forces fast_path_granted=0, sets
 *   admission_status='REJECTED_BLOCKING', inserts and commits row, and returns
 *   CBM_GOV_ERR_BLOCKING_VIOLATIONS.
 * - If blocking_violations_json is empty and warnings_json is non-empty: sets
 *   admission_status='ADMITTED_WITH_WARNINGS', inserts and commits row, returns CBM_GOV_OK.
 * - If both are empty: sets admission_status='ADMITTED_CLEAN', inserts and commits, returns CBM_GOV_OK. */
cbm_gov_status_t cbm_gov_record_manifest(cbm_gov_store_t *store,
                                         const cbm_admission_manifest_t *manifest,
                                         const char *expected_snapshot_id,
                                         const char *expected_graph_generation,
                                         const char *expected_rules_revision,
                                         const char *expected_coverage_revision,
                                         bool server_fast_path_eligible,
                                         int64_t now_epoch,
                                         char admission_status_out[32],
                                         bool *out_idempotent_replay);

/* Lookup a recorded manifest by manifest_id. */
cbm_gov_status_t cbm_gov_get_manifest(cbm_gov_store_t *store,
                                      const char *manifest_id,
                                      cbm_admission_manifest_t *out_manifest);

void cbm_gov_manifest_free(cbm_admission_manifest_t *manifest);

/* Host/Test-only clock override (never exposed as MCP tool JSON argument).
 * When override_epoch > 0, cbm_gov_now() returns override_epoch; when <= 0, returns time(NULL). */
void cbm_gov_set_clock_override_for_tests(int64_t override_epoch);
int64_t cbm_gov_now(void);

#ifdef __cplusplus
}
#endif

#endif /* CBM_GOVERNANCE_STORE_H */
