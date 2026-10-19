#ifndef CBM_HARNESS_V2_H
#define CBM_HARNESS_V2_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "store/store.h"
#include "store/governance_store.h"
#include "yyjson/yyjson.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CBM_EFFECT_ABSENT  = 0,
    CBM_EFFECT_UNKNOWN = 1,
    CBM_EFFECT_PRESENT = 2
} cbm_effect_tristate_t;

typedef struct {
    cbm_effect_tristate_t writes_db;
    cbm_effect_tristate_t network_io;
    cbm_effect_tristate_t fs_mutation;
    cbm_effect_tristate_t spawns_process;
    cbm_effect_tristate_t global_state_write;
    cbm_effect_tristate_t throws_or_panics;
    cbm_effect_tristate_t dynamic_dispatch;
    char *uncertainty_reasons_json; /* JSON array of strings, sorted */
} cbm_effect_fingerprint_t;

typedef struct {
    char *pole_id;
    char *qualified_name;
    char *file_path;
    int start_line;
    int end_line;
    int hop_distance;
    char *focus_mode; /* "integral" or "skeleton" */
    char *code;       /* Populated when focus_mode == "integral" */
    char *skeleton_signature;
    char *skeleton_doc_summary;
    cbm_effect_fingerprint_t effects;
} cbm_harness_candidate_t;

typedef struct {
    char *seam_id;
    char *caller_pole;
    char *callee_pole;
    char *edge_type;
    char *seam_kind;      /* "pure_call", "io_boundary", "db_transaction", "ipc_rpc", "shared_state" */
    char *contract_shape; /* signature / contract summary */
    char *risk_level;     /* "low", "medium", "high" */
    char *uncertainty_flags_json;
} cbm_harness_seam_t;

typedef struct {
    bool eligible;
    bool all_seams_resolved;
    bool zero_uncertainty_effects;
    bool coverage_complete;
    bool no_blocking_tradition_conflicts;
    bool change_kind_compatible;
    char *eligibility_reasons_json; /* JSON array of strings */
    char *disqualifying_reasons_json; /* JSON array of strings */
    char *snapshot_id;
    char *graph_generation;
    char *rules_revision;
    char *coverage_revision;
    char *bundle_hash;
} cbm_fast_path_verdict_t;

/* Canonical JSON serialization (RFC 8785 style: recursively sorted object keys,
 * compact formatting without extraneous whitespace).
 * Caller must free() *out_canonical. */
int cbm_canonical_json_from_val(yyjson_val *val, char **out_canonical);
int cbm_canonical_json(const char *json_str, char **out_canonical);

/* Compute SHA-256 over canonicalized JSON string. Out buffer is 64 hex chars + NUL. */
int cbm_canonical_json_sha256(const char *json_str, char out_hex[65]);

/* Validation helpers (Tasks 01, U01-U04) */
bool cbm_harness_validate_sha256_hex(const char *hex_str);
bool cbm_harness_validate_span(int start_line, int end_line);
bool cbm_harness_validate_k_hop(int k_hop);

/* Composite Snapshot Identity (Task 03, U01, U02, I10-I12):
 * snapshot_id = SHA256(graph_generation + ":" + rules_revision + ":" + coverage_revision) */
int cbm_harness_compute_snapshot_id(const char *graph_generation,
                                    const char *rules_revision,
                                    const char *coverage_revision,
                                    char out_snapshot_id[65]);

/* Compute coverage_revision from store coverage tables + file hashes for a project. */
int cbm_harness_compute_coverage_revision(cbm_store_t *store,
                                          const char *project,
                                          const char *const *target_files,
                                          int target_file_count,
                                          char out_coverage_rev[65],
                                          bool *out_coverage_complete,
                                          char **out_coverage_digest_json);

/* Tri-State Effect Lattice operations (Task 05, U05-U08, F01):
 * Dominance order: PRESENT (2) > UNKNOWN (1) > ABSENT (0) */
cbm_effect_tristate_t cbm_effect_join(cbm_effect_tristate_t a, cbm_effect_tristate_t b);
const char *cbm_effect_tristate_str(cbm_effect_tristate_t st);
cbm_effect_tristate_t cbm_effect_tristate_parse(const char *str);
bool cbm_effect_has_any_unknown(const cbm_effect_fingerprint_t *fp);
bool cbm_effect_has_any_side_effect(const cbm_effect_fingerprint_t *fp);

/* Classify intrinsic symbol effects from source / node properties / calls,
 * and aggregate across call subgraph with cycle-safe fixed-point propagation. */
int cbm_harness_compute_node_effects(cbm_store_t *store,
                                     const char *project,
                                     int64_t node_id,
                                     int max_depth,
                                     cbm_effect_fingerprint_t *out_fp);

void cbm_effect_fingerprint_free(cbm_effect_fingerprint_t *fp);

/* Extract a complete HarnessEvidenceBundle v2 JSON document (Tasks 06, 07, U09, I01-I06, F07).
 * Caller must free() *out_bundle_json. */
int cbm_harness_extract_bundle(cbm_store_t *store,
                               cbm_gov_store_t *gov_store,
                               const char *project,
                               const char *task_id,
                               const char *change_kind,
                               const char *const *seed_poles,
                               int seed_pole_count,
                               int k_hop,
                               int64_t evaluation_time,
                               char **out_bundle_json,
                               char out_bundle_hash[65],
                               char out_snapshot_id[65]);

/* Evaluate fail-closed Fast-Path eligibility on a bundle JSON or store state (Task 08, U18-U19, F02, F04). */
int cbm_harness_evaluate_fast_path(cbm_store_t *store,
                                   cbm_gov_store_t *gov_store,
                                   const char *project,
                                   const char *bundle_json,
                                   const char *change_kind_override,
                                   int64_t evaluation_time,
                                   cbm_fast_path_verdict_t *out_verdict,
                                   char **out_verdict_json);

void cbm_fast_path_verdict_free(cbm_fast_path_verdict_t *v);

#ifdef __cplusplus
}
#endif

#endif /* CBM_HARNESS_V2_H */
