#include "admission_gate.h"
#include "../foundation/log.h"
#include "../union/union_refusal.h"
#include "../core/cbm_uri.h"
#if __has_include(<yyjson/yyjson.h>)
  #include <yyjson/yyjson.h>
#elif __has_include("yyjson/yyjson.h")
  #include "yyjson/yyjson.h"
#elif __has_include("yyjson.h")
  #include "yyjson.h"
#else
  #include <yyjson/yyjson.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && !defined(strcasecmp)
#define strcasecmp _stricmp
#endif

static int cbm_node_exists_in_base(sqlite3 *base_db, const char *uri, char *out_err, size_t err_sz) {
    if (out_err && err_sz > 0) out_err[0] = '\0';
    if (!base_db || !uri) return -1;
    sqlite3_stmt *stmt = NULL;
    int prep_rc = sqlite3_prepare_v2(base_db, "SELECT 1 FROM nodes WHERE cbm_uri = ? OR qualified_name = ? LIMIT 1", -1, &stmt, NULL);
    if (prep_rc != SQLITE_OK) {
        if (out_err && err_sz > 0) {
            snprintf(out_err, err_sz, "%s", sqlite3_errmsg(base_db));
        }
        return -1;
    }
    sqlite3_bind_text(stmt, 1, uri, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, uri, -1, SQLITE_STATIC);
    int step_rc = sqlite3_step(stmt);
    if (step_rc != SQLITE_ROW && step_rc != SQLITE_DONE) {
        if (out_err && err_sz > 0) {
            snprintf(out_err, err_sz, "%s", sqlite3_errmsg(base_db));
        }
        sqlite3_finalize(stmt);
        return -1;
    }
    sqlite3_finalize(stmt);
    if (step_rc == SQLITE_ROW) return 1;
    return 0;
}

static bool cbm_path_has_traversal(const char *p) {
    if (!p) return false;
    const char *cur = p;
    while (*cur) {
        if (cur[0] == '.' && cur[1] == '.') {
            bool left_boundary = (cur == p || *(cur - 1) == '/' || *(cur - 1) == '\\');
            bool right_boundary = (cur[2] == '\0' || cur[2] == '/' || cur[2] == '\\');
            if (left_boundary && right_boundary) {
                return true;
            }
        }
        cur++;
    }
    return false;
}

static const char *cbm_strip_leading_curdir(const char *p) {
    if (!p) return NULL;
    while (p[0] == '.' && (p[1] == '/' || p[1] == '\\')) {
        /* Do not consume if followed by another slash, e.g. .// */
        if (p[2] == '/' || p[2] == '\\') {
            break;
        }
        p += 2;
    }
    return p;
}

bool cbm_paths_match_strict(const char *pa, const char *pb) {
    if (!pa || !pb) return false;

    /* Absolute vs Relative asymmetry check: both must start with slash or neither */
    bool pa_is_abs = (pa[0] == '/' || pa[0] == '\\');
    bool pb_is_abs = (pb[0] == '/' || pb[0] == '\\');
    if (pa_is_abs != pb_is_abs) {
        return false;
    }

    /* Reject any directory traversal segments (..) in either path */
    if (cbm_path_has_traversal(pa) || cbm_path_has_traversal(pb)) {
        return false;
    }

    /* Strip only benign leading "./" or ".\\" prefixes, preserving hidden files like ".hidden.c" */
    pa = cbm_strip_leading_curdir(pa);
    pb = cbm_strip_leading_curdir(pb);

    /* Strict character-by-character comparison normalizing path separators */
    while (*pa && *pb) {
        char c1 = (*pa == '\\') ? '/' : *pa;
        char c2 = (*pb == '\\') ? '/' : *pb;
        if (c1 != c2) return false;
        pa++;
        pb++;
    }
    /* Both strings must reach null terminator simultaneously */
    return (*pa == '\0' && *pb == '\0');
}

int cbm_find_matching_anchor(const char *uri, const char *lbl,
                             const char *expected_repo,
                             const TwoTierAnchor *anchors, size_t anchor_count) {
    if (!uri || !anchors || anchor_count == 0) return -1;

    CbmUri parsed;
    bool has_parsed = (cbm_uri_parse(uri, &parsed) == CBM_URI_OK);

    const char *node_symbol = NULL;
    const char *node_path = NULL;
    char path_buf[512] = {0};

    if (has_parsed) {
        /* Complete Identity: Mandatory repo verification if expected_repo is specified */
        if (expected_repo && expected_repo[0] != '\0') {
            if (parsed.repo[0] == '\0' || strcmp(parsed.repo, expected_repo) != 0) {
                /* Node belongs to a different project/repo or lacks repo */
                return -1;
            }
        }

        node_symbol = parsed.symbol;
        node_path = parsed.path;
    } else {
        /* If expected_repo is specified, repo-less URIs are strictly rejected */
        if (expected_repo && expected_repo[0] != '\0') {
            return -1;
        }

        const char *hash_pos = strchr(uri, '#');
        if (hash_pos && *(hash_pos + 1)) {
            node_symbol = hash_pos + 1;
            size_t plen = (size_t)(hash_pos - uri);
            if (plen >= sizeof(path_buf)) plen = sizeof(path_buf) - 1;
            strncpy(path_buf, uri, plen);
            path_buf[plen] = '\0';
            node_path = path_buf;
        } else if (lbl && lbl[0]) {
            node_symbol = lbl;
        } else {
            node_symbol = uri;
        }
    }

    if (!node_symbol || node_symbol[0] == '\0') {
        return -1;
    }

    /* Reject if node_path contains directory traversal or is invalid */
    if (node_path && cbm_path_has_traversal(node_path)) {
        return -1;
    }

    for (size_t a = 0; a < anchor_count; a++) {
        if (anchors[a].symbol_name[0] == '\0') continue;

        /* 1. Exact symbol identity: divergent URI symbol can never be overridden by label */
        if (strcmp(node_symbol, anchors[a].symbol_name) != 0) {
            continue;
        }

        /* 2. File path confinement: both sides must specify path and match strictly, or neither */
        bool anchor_has_path = (anchors[a].file_path[0] != '\0');
        bool node_has_path = (node_path != NULL && node_path[0] != '\0');

        if (anchor_has_path != node_has_path) {
            /* Path confinement mismatch: one is file-scoped while other lacks path */
            continue;
        }

        if (anchor_has_path && node_has_path) {
            if (!cbm_paths_match_strict(node_path, anchors[a].file_path)) {
                /* Node is from a different file or subtree */
                continue;
            }
        }

        return (int)a;
    }

    return -1;
}

int cbm_admission_gate_init(AdmissionGate *gate, const char *project_id, uint64_t generation) {
    if (!gate) return -1;
    memset(gate, 0, sizeof(*gate));
    if (project_id) {
        snprintf(gate->project_id, sizeof(gate->project_id), "%s", project_id);
    }
    gate->base_generation = generation;
    gate->active_recalls_count = 0;
    gate->base_db = NULL;
    return 0;
}

void cbm_admission_gate_set_base_db(AdmissionGate *gate, sqlite3 *base_db) {
    if (gate) {
        gate->base_db = base_db;
    }
}

int cbm_admission_gate_check_concurrent_conflicts(
    AdmissionGate *gate,
    HorizonConnectionPool *pool,
    const char *current_horizon_id,
    const TwoTierAnchor *anchors,
    size_t anchor_count,
    HorizonConflictReport *out_report)
{
    if (!gate || !pool || !current_horizon_id || !out_report) return CBM_ADMISSION_ERR_INVALID_PARAMS;
    memset(out_report, 0, sizeof(*out_report));

    ActiveHorizonLiveness *liveness = NULL;
    size_t active_count = 0;
    char enum_err[512] = {0};
    int enum_rc = cbm_horizon_pool_get_active_alive(
        pool,
        gate->project_id[0] ? gate->project_id : "default",
        &liveness,
        &active_count,
        enum_err,
        sizeof(enum_err));
    if (enum_rc != 0) {
        snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", current_horizon_id);
        snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "%s", enum_err[0] ? enum_err : "failed enumerating active horizons");
        return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
    }

    bool semantic_found = false;

    TwoTierAnchor *local_anchors = NULL;
    size_t local_count = 0;
    if (!anchors || anchor_count == 0) {
        sqlite3 *cur_db = NULL;
        if (cbm_horizon_pool_get(pool, current_horizon_id, &cur_db) != 0 || !cur_db) {
            snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", current_horizon_id);
            snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "unable to access horizon db");
            if (liveness) free(liveness);
            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
        }
        sqlite3_stmt *stmt = NULL;
        int prep_rc = sqlite3_prepare_v2(cur_db, "SELECT cbm_uri FROM symbolic_nodes", -1, &stmt, NULL);
        if (prep_rc != SQLITE_OK) {
            char safe_err[512] = {0};
            snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(cur_db));
            snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", current_horizon_id);
            snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "%s", safe_err);
            if (liveness) free(liveness);
            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
        }
        int step_rc;
        while ((step_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            local_count++;
        }
        if (step_rc != SQLITE_DONE) {
            char safe_err[512] = {0};
            snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(cur_db));
            sqlite3_finalize(stmt);
            snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", current_horizon_id);
            snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "%s", safe_err);
            if (liveness) free(liveness);
            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
        }
        sqlite3_finalize(stmt);

        if (local_count > 0) {
            local_anchors = (TwoTierAnchor *)calloc(local_count, sizeof(TwoTierAnchor));
            if (!local_anchors) {
                if (liveness) free(liveness);
                return CBM_ADMISSION_ERR_INVALID_STATE;
            }
            prep_rc = sqlite3_prepare_v2(cur_db, "SELECT cbm_uri FROM symbolic_nodes", -1, &stmt, NULL);
            if (prep_rc != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(cur_db));
                free(local_anchors);
                snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", current_horizon_id);
                snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "%s", safe_err);
                if (liveness) free(liveness);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }
            size_t idx = 0;
            while ((step_rc = sqlite3_step(stmt)) == SQLITE_ROW && idx < local_count) {
                const char *uri = (const char *)sqlite3_column_text(stmt, 0);
                if (uri) {
                    const char *hash_pos = strchr(uri, '#');
                    if (hash_pos) {
                        snprintf(local_anchors[idx].file_path, sizeof(local_anchors[idx].file_path), "%.*s", (int)(hash_pos - uri), uri);
                        snprintf(local_anchors[idx].symbol_name, sizeof(local_anchors[idx].symbol_name), "%s", hash_pos + 1);
                    } else {
                        snprintf(local_anchors[idx].file_path, sizeof(local_anchors[idx].file_path), "%s", uri);
                    }
                }
                idx++;
            }
            if (step_rc != SQLITE_DONE && idx < local_count) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(cur_db));
                sqlite3_finalize(stmt);
                free(local_anchors);
                snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", current_horizon_id);
                snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "%s", safe_err);
                if (liveness) free(liveness);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }
            sqlite3_finalize(stmt);
        }
        anchors = local_anchors;
        anchor_count = local_count;
    }

    for (size_t i = 0; i < active_count; i++) {
        if (strcmp(liveness[i].horizon_id, current_horizon_id) == 0) continue;

        sqlite3 *sib_db = NULL;
        if (cbm_horizon_pool_get(pool, liveness[i].horizon_id, &sib_db) != 0 || !sib_db) {
            snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", liveness[i].horizon_id);
            snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "unable to access sibling horizon db");
            if (local_anchors) free(local_anchors);
            if (liveness) free(liveness);
            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
        }

        // Check physical collision with query pushdown
        sqlite3_stmt *stmt = NULL;
        const char *q_phys = "SELECT cbm_uri FROM symbolic_nodes WHERE cbm_uri LIKE ? LIMIT 1";
        int prep_rc = sqlite3_prepare_v2(sib_db, q_phys, -1, &stmt, NULL);
        if (prep_rc != SQLITE_OK) {
            char safe_err[512] = {0};
            snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(sib_db));
            snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", liveness[i].horizon_id);
            snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "%s", safe_err);
            if (local_anchors) free(local_anchors);
            if (liveness) free(liveness);
            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
        }

        for (size_t a = 0; a < anchor_count; a++) {
            const char *a_file = anchors[a].file_path;
            const char *a_sym = anchors[a].symbol_name;
            if (!a_file) continue;

            char pattern[1024];
            snprintf(pattern, sizeof(pattern), "%%%s%%", a_file);
            sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
            int step_rc = sqlite3_step(stmt);
            if (step_rc == SQLITE_ROW) {
                snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", liveness[i].horizon_id);
                snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "%s", a_file);
                snprintf(out_report->conflicting_symbol, sizeof(out_report->conflicting_symbol), "%s", a_sym ? a_sym : "");
                out_report->is_semantic_only = false;
                sqlite3_finalize(stmt);
                if (local_anchors) free(local_anchors);
                if (liveness) free(liveness);
                return CBM_ADMISSION_ERR_CONCURRENT_CONFLICT;
            } else if (step_rc != SQLITE_DONE) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(sib_db));
                sqlite3_finalize(stmt);
                snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", liveness[i].horizon_id);
                snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "%s", safe_err);
                if (local_anchors) free(local_anchors);
                if (liveness) free(liveness);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }
            sqlite3_reset(stmt);
        }
        sqlite3_finalize(stmt);
        stmt = NULL;

        // Check semantic collision with query pushdown
        if (!semantic_found) {
            const char *q_sem = "SELECT source_uri, target_uri FROM virtual_edges WHERE (source_uri LIKE ? AND source_uri LIKE ?) OR (target_uri LIKE ? AND target_uri LIKE ?) LIMIT 1";
            prep_rc = sqlite3_prepare_v2(sib_db, q_sem, -1, &stmt, NULL);
            if (prep_rc != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(sib_db));
                snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", liveness[i].horizon_id);
                snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "%s", safe_err);
                if (local_anchors) free(local_anchors);
                if (liveness) free(liveness);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }

            for (size_t a = 0; a < anchor_count; a++) {
                const char *a_file = anchors[a].file_path;
                const char *a_sym = anchors[a].symbol_name;
                if (!a_file || !a_sym || a_sym[0] == '\0') continue;

                char p_file[1024];
                char p_sym[512];
                snprintf(p_file, sizeof(p_file), "%%%s%%", a_file);
                snprintf(p_sym, sizeof(p_sym), "%%%s%%", a_sym);
                sqlite3_bind_text(stmt, 1, p_file, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, p_sym, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 3, p_file, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 4, p_sym, -1, SQLITE_TRANSIENT);
                int step_rc = sqlite3_step(stmt);
                if (step_rc == SQLITE_ROW) {
                    snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", liveness[i].horizon_id);
                    snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "%s", a_file);
                    snprintf(out_report->conflicting_symbol, sizeof(out_report->conflicting_symbol), "%s", a_sym);
                    out_report->is_semantic_only = true;
                    semantic_found = true;
                    break;
                } else if (step_rc != SQLITE_DONE) {
                    char safe_err[512] = {0};
                    snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(sib_db));
                    sqlite3_finalize(stmt);
                    snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", liveness[i].horizon_id);
                    snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "%s", safe_err);
                    if (local_anchors) free(local_anchors);
                    if (liveness) free(liveness);
                    return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                }
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);
            stmt = NULL;
        }
    }

    if (local_anchors) free(local_anchors);
    if (liveness) free(liveness);

    if (semantic_found) {
        return CBM_ADMISSION_ERR_CONCURRENT_CONFLICT;
    }

    return CBM_ADMISSION_OK;
}

int cbm_promote_horizon(AdmissionGate *gate,
                        HorizonConnectionPool *pool,
                        const char *repo_root,
                        const char *horizon_id,
                        const TwoTierAnchor *anchors,
                        size_t anchor_count,
                        char *out_error,
                        size_t err_sz) {
    if (!horizon_id) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "Missing horizon_id");
        return CBM_ADMISSION_ERR_HORIZON_NOT_FOUND;
    }
    if (!anchors || anchor_count == 0) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "Promotion requires at least one anchor");
        return CBM_ADMISSION_ERR_INVALID_PARAMS;
    }
    if (!gate) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "Missing admission gate");
        return CBM_ADMISSION_ERR_INVALID_STATE;
    }

    /* Verify horizon exists in pool before proceeding */
    if (pool) {
        sqlite3 *check_db = NULL;
        if (cbm_horizon_pool_get(pool, horizon_id, &check_db) != 0 || !check_db) {
            if (out_error && err_sz > 0) {
                snprintf(out_error, err_sz, "Horizon '%s' not found", horizon_id);
            }
            return CBM_ADMISSION_ERR_HORIZON_NOT_FOUND;
        }
    }

    if (!gate->base_db) {
        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "Persistent base database is unavailable");
        return CBM_ADMISSION_ERR_BASE_UNAVAILABLE;
    }

    HorizonConflictReport conflict_report;
    int conflict_rc = cbm_admission_gate_check_concurrent_conflicts(gate, pool, horizon_id, anchors, anchor_count, &conflict_report);
    if (conflict_rc == CBM_ADMISSION_ERR_CONCURRENT_CONFLICT) {
        if (out_error && err_sz > 0) {
            snprintf(out_error, err_sz, "CONCURRENT_CONFLICT: conflict with horizon %s on file %s", conflict_report.conflicting_horizon, conflict_report.conflicting_file);
        }
        return CBM_ADMISSION_ERR_CONCURRENT_CONFLICT;
    }
    if (conflict_rc != CBM_ADMISSION_OK) {
        if (out_error && err_sz > 0) {
            snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: concurrent conflict scan failed for horizon %s: %s",
                     conflict_report.conflicting_horizon[0] ? conflict_report.conflicting_horizon : horizon_id,
                     conflict_report.conflicting_file[0] ? conflict_report.conflicting_file : "query error");
        }
        return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
    }

    /* Allocate relocation tracking buffer */
    TwoTierAnchorRelocation stack_relocs[32];
    TwoTierAnchorRelocation *relocs = stack_relocs;
    memset(stack_relocs, 0, sizeof(stack_relocs));
    if (anchor_count > 32) {
        relocs = (TwoTierAnchorRelocation *)calloc(anchor_count, sizeof(TwoTierAnchorRelocation));
        if (!relocs) {
            if (out_error && err_sz > 0) snprintf(out_error, err_sz, "Out of memory");
            return CBM_ADMISSION_ERR_INVALID_STATE;
        }
    }

    /* Validate each anchor against the codebase files without mutating const input */
    if (anchors && anchor_count > 0) {
        for (size_t i = 0; i < anchor_count; i++) {
            bool ok = false;
            int rc = cbm_verify_two_tier_anchor(repo_root, &anchors[i], &ok, &relocs[i]);
            if (rc != 0 || !ok) {
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "ANCHOR_DRIFT: anchor '%s' in '%s' diverged from filesystem",
                             anchors[i].symbol_name, anchors[i].file_path);
                }
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_ANCHOR_DRIFT;
            }
        }
    }

    /* Anchors verified! Transition horizon status to PROMOTED */
    if (pool) {
        /* Consolidate promoted nodes and virtual edges into the persistent base graph. */
        if (gate->base_db) {
            char horizon_path[CBM_PATH_MAX] = {0};
            if (cbm_horizon_pool_get_path(pool, horizon_id, horizon_path, sizeof(horizon_path)) != 0) {
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "HORIZON_NOT_FOUND: could not resolve path for horizon %s", horizon_id);
                }
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_HORIZON_NOT_FOUND;
            }

            /* Ensure horizon db is checkpointed and close pool handle to allow single-writer exclusive access */
            for (size_t i = 0; i < CBM_MAX_HORIZON_FDS; i++) {
                if (pool->open_handles[i] && strcmp(pool->active_ids[i], horizon_id) == 0) {
                    sqlite3_exec(pool->open_handles[i], "PRAGMA wal_checkpoint(TRUNCATE);", NULL, NULL, NULL);
                    sqlite3_close_v2(pool->open_handles[i]);
                    pool->open_handles[i] = NULL;
                    pool->active_ids[i][0] = '\0';
                    pool->lru_ticks[i] = 0;
                    pool->count--;
                    break;
                }
            }

            /* Persistent file check: 2PC crash atomicity requires a persistent file for the main database */
            const char *base_fn = sqlite3_db_filename(gate->base_db, "main");
            if (!base_fn || base_fn[0] == '\0' || strcmp(base_fn, ":memory:") == 0) {
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "COMMIT_FAILED: 2PC crash atomicity requires a persistent file database for main, but :memory: or unnamed database detected");
                }
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }

            /* Configure base_db to use Rollback Journaling (DELETE mode) and synchronous FULL for 2PC crash atomicity */
            if (sqlite3_exec(gate->base_db, "PRAGMA main.journal_mode = DELETE;", NULL, NULL, NULL) != SQLITE_OK ||
                sqlite3_exec(gate->base_db, "PRAGMA main.synchronous = FULL;", NULL, NULL, NULL) != SQLITE_OK) {
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "COMMIT_FAILED: failed configuring rollback journaling on main database: %s", sqlite3_errmsg(gate->base_db));
                }
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }

            /* Ensure h_db is not previously attached on base_db connection */
            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);

            /* Attach the horizon database to gate->base_db for atomic two-phase commit */
            sqlite3_stmt *att_stmt = NULL;
            if (sqlite3_prepare_v2(gate->base_db, "ATTACH DATABASE ? AS h_db;", -1, &att_stmt, NULL) != SQLITE_OK) {
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed to prepare ATTACH statement: %s",
                             sqlite3_errmsg(gate->base_db));
                }
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }
            sqlite3_bind_text(att_stmt, 1, horizon_path, -1, SQLITE_STATIC);
            int att_rc = sqlite3_step(att_stmt);
            if (att_rc != SQLITE_DONE) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_finalize(att_stmt);
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed to attach horizon database %s: %s",
                             horizon_path, safe_err);
                }
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }
            sqlite3_finalize(att_stmt);

            /* Configure attached h_db to use Rollback Journaling (DELETE mode) and synchronous FULL */
            if (sqlite3_exec(gate->base_db, "PRAGMA h_db.wal_checkpoint(TRUNCATE);", NULL, NULL, NULL) != SQLITE_OK ||
                sqlite3_exec(gate->base_db, "PRAGMA h_db.journal_mode = DELETE;", NULL, NULL, NULL) != SQLITE_OK ||
                sqlite3_exec(gate->base_db, "PRAGMA h_db.synchronous = FULL;", NULL, NULL, NULL) != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "COMMIT_FAILED: failed configuring rollback journaling on attached horizon: %s", safe_err);
                }
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }

            /* Strict Positive 2PC Invariants:
             * 1. main.journal_mode MUST be positively 'delete'
             * 2. h_db.journal_mode MUST be positively 'delete'
             * 3. main.synchronous MUST be >= 2 (FULL or EXTRA)
             * 4. h_db.synchronous MUST be >= 2 (FULL or EXTRA)
             */
            bool durability_ok = false;
            sqlite3_stmt *chk_stmt = NULL;
            char main_jm[32] = {0};
            char h_jm[32] = {0};
            int main_sync = 0;
            int h_sync = 0;

            if (sqlite3_prepare_v2(gate->base_db, "PRAGMA main.journal_mode;", -1, &chk_stmt, NULL) != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "COMMIT_FAILED: failed checking main.journal_mode: %s", safe_err);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }
            int step_mj = sqlite3_step(chk_stmt);
            if (step_mj != SQLITE_ROW) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_finalize(chk_stmt);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "COMMIT_FAILED: failed reading main.journal_mode: %s", safe_err);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }
            const char *jm = (const char *)sqlite3_column_text(chk_stmt, 0);
            if (jm) snprintf(main_jm, sizeof(main_jm), "%s", jm);
            sqlite3_finalize(chk_stmt);

            if (sqlite3_prepare_v2(gate->base_db, "PRAGMA h_db.journal_mode;", -1, &chk_stmt, NULL) != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "COMMIT_FAILED: failed checking h_db.journal_mode: %s", safe_err);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }
            int step_hj = sqlite3_step(chk_stmt);
            if (step_hj != SQLITE_ROW) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_finalize(chk_stmt);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "COMMIT_FAILED: failed reading h_db.journal_mode: %s", safe_err);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }
            jm = (const char *)sqlite3_column_text(chk_stmt, 0);
            if (jm) snprintf(h_jm, sizeof(h_jm), "%s", jm);
            sqlite3_finalize(chk_stmt);

            if (sqlite3_prepare_v2(gate->base_db, "PRAGMA main.synchronous;", -1, &chk_stmt, NULL) != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "COMMIT_FAILED: failed checking main.synchronous: %s", safe_err);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }
            int step_ms = sqlite3_step(chk_stmt);
            if (step_ms != SQLITE_ROW) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_finalize(chk_stmt);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "COMMIT_FAILED: failed reading main.synchronous: %s", safe_err);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }
            main_sync = sqlite3_column_int(chk_stmt, 0);
            sqlite3_finalize(chk_stmt);

            if (sqlite3_prepare_v2(gate->base_db, "PRAGMA h_db.synchronous;", -1, &chk_stmt, NULL) != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "COMMIT_FAILED: failed checking h_db.synchronous: %s", safe_err);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }
            int step_hs = sqlite3_step(chk_stmt);
            if (step_hs != SQLITE_ROW) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_finalize(chk_stmt);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "COMMIT_FAILED: failed reading h_db.synchronous: %s", safe_err);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }
            h_sync = sqlite3_column_int(chk_stmt, 0);
            sqlite3_finalize(chk_stmt);

            bool main_is_delete = (strcasecmp(main_jm, "delete") == 0);
            bool h_is_delete = (strcasecmp(h_jm, "delete") == 0);
            if (main_is_delete && h_is_delete && main_sync >= 2 && h_sync >= 2) {
                durability_ok = true;
            }

            if (!durability_ok) {
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz,
                             "COMMIT_FAILED: 2PC crash atomicity requires positive DELETE rollback journal and synchronous>=FULL on all databases (main: mode=%s, sync=%d; h_db: mode=%s, sync=%d)",
                             main_jm[0] ? main_jm : "unknown", main_sync,
                             h_jm[0] ? h_jm : "unknown", h_sync);
                }
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }

            if (sqlite3_exec(gate->base_db, "BEGIN IMMEDIATE;", NULL, NULL, NULL) != SQLITE_OK) {
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "CONCURRENT_CONFLICT: could not acquire exclusive lock");
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_CONCURRENT_CONFLICT;
            }
            
            /* TOCTOU Protection: Verify base_generation hasn't drifted while checking anchors */
            bool generation_drifted = false;
            sqlite3_stmt *gen_stmt = NULL;
            if (sqlite3_exec(gate->base_db, "CREATE TABLE IF NOT EXISTS generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL) != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed creating generation_log table: %s", safe_err);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }
            int prep_gen = sqlite3_prepare_v2(gate->base_db, "SELECT MAX(generation) FROM generation_log", -1, &gen_stmt, NULL);
            if (prep_gen != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed to prepare generation check: %s", safe_err);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }
            int step_gen = sqlite3_step(gen_stmt);
            if (step_gen == SQLITE_ROW) {
                if (sqlite3_column_type(gen_stmt, 0) != SQLITE_NULL) {
                    uint64_t current_gen = (uint64_t)sqlite3_column_int64(gen_stmt, 0);
                    if (current_gen > gate->base_generation) {
                        generation_drifted = true;
                    }
                }
            } else if (step_gen != SQLITE_DONE) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_finalize(gen_stmt);
                sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: error reading generation_log: %s", safe_err);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }
            sqlite3_finalize(gen_stmt);
            if (generation_drifted) {
                sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "STALE_BASE: base_generation drifted during anchor verification (TOCTOU generation_check)");
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_CONCURRENT_CONFLICT;
            }
            
            if (sqlite3_exec(gate->base_db, "CREATE TABLE IF NOT EXISTS projects (name TEXT PRIMARY KEY, root_dir TEXT);", NULL, NULL, NULL) != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed creating projects table: %s", safe_err);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }
            sqlite3_stmt *p_stmt = NULL;
            if (gate->project_id[0]) {
                if (sqlite3_prepare_v2(gate->base_db, "INSERT OR IGNORE INTO projects (name, root_dir) VALUES (?, '');", -1, &p_stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(p_stmt, 1, gate->project_id, -1, SQLITE_STATIC);
                    int step_p = sqlite3_step(p_stmt);
                    if (step_p != SQLITE_DONE) {
                        char safe_err[512] = {0};
                        snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                        sqlite3_finalize(p_stmt);
                        sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                        sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                        if (out_error && err_sz > 0) snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed inserting project: %s", safe_err);
                        if (relocs != stack_relocs) free(relocs);
                        return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                    }
                    sqlite3_finalize(p_stmt);
                } else {
                    char safe_err[512] = {0};
                    snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                    sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                    sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                    if (out_error && err_sz > 0) snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed preparing project statement: %s", safe_err);
                    if (relocs != stack_relocs) free(relocs);
                    return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                }
            } else {
                if (sqlite3_exec(gate->base_db, "INSERT OR IGNORE INTO projects (name, root_dir) VALUES ('default', '');", NULL, NULL, NULL) != SQLITE_OK) {
                    char safe_err[512] = {0};
                    snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                    sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                    sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                    if (out_error && err_sz > 0) snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed inserting default project: %s", safe_err);
                    if (relocs != stack_relocs) free(relocs);
                    return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                }
            }
            if (sqlite3_exec(gate->base_db, "CREATE TABLE IF NOT EXISTS virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL) != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed creating virtual_edges in base_db: %s", safe_err);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }

            /* Consolidate symbolic_nodes reading directly from h_db under single transactional snapshot */
            sqlite3_stmt *stmt = NULL;
            const char *sel_nodes = "SELECT cbm_uri, label, code_snippet FROM h_db.symbolic_nodes WHERE epistemic_status = 'ACCEPTED';";
            int prep_rc = sqlite3_prepare_v2(gate->base_db, sel_nodes, -1, &stmt, NULL);
            if (prep_rc != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed to prepare symbolic_nodes query (sqlite error %d: %s)",
                             prep_rc, safe_err);
                }
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }

            /* Schema introspection on base_db nodes table for coordinate storage support */
            bool base_has_byte_start = false;
            bool base_has_byte_len = false;
            bool base_has_properties = false;
            bool base_has_cbm_uri = false;
            bool base_has_qualified_name = false;
            sqlite3_stmt *info_stmt = NULL;
            int step_info = SQLITE_DONE;
            if (sqlite3_prepare_v2(gate->base_db, "PRAGMA table_info(nodes);", -1, &info_stmt, NULL) == SQLITE_OK) {
                while ((step_info = sqlite3_step(info_stmt)) == SQLITE_ROW) {
                    const char *col_name = (const char *)sqlite3_column_text(info_stmt, 1);
                    if (col_name) {
                        if (strcmp(col_name, "byte_start") == 0) base_has_byte_start = true;
                        else if (strcmp(col_name, "byte_len") == 0) base_has_byte_len = true;
                        else if (strcmp(col_name, "properties") == 0) base_has_properties = true;
                        else if (strcmp(col_name, "cbm_uri") == 0) base_has_cbm_uri = true;
                        else if (strcmp(col_name, "qualified_name") == 0) base_has_qualified_name = true;
                    }
                }
                if (step_info != SQLITE_DONE) {
                    char safe_err[512] = {0};
                    snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                    sqlite3_finalize(info_stmt);
                    sqlite3_finalize(stmt);
                    sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                    sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                    if (out_error && err_sz > 0) snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: error reading table_info(nodes): %s", safe_err);
                    if (relocs != stack_relocs) free(relocs);
                    return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                }
                sqlite3_finalize(info_stmt);
            }

            size_t admitted_cap = 256;
            char (*admitted_uris)[CBM_URI_MAX_LEN] = (char (*)[CBM_URI_MAX_LEN])malloc(admitted_cap * CBM_URI_MAX_LEN);
            size_t admitted_count = 0;

            int step_sel_rc = SQLITE_DONE;
            while ((step_sel_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                const char *uri = (const char *)sqlite3_column_text(stmt, 0);
                const char *lbl = (const char *)sqlite3_column_text(stmt, 1);
                const char *code = (const char *)sqlite3_column_text(stmt, 2);

                char safe_uri[CBM_URI_MAX_LEN] = {0};
                if (uri) snprintf(safe_uri, sizeof(safe_uri), "%s", uri);
                char safe_lbl[256] = {0};
                if (lbl) snprintf(safe_lbl, sizeof(safe_lbl), "%s", lbl);

                int matched_anchor_idx = -1;
                if (anchors && anchor_count > 0) {
                    matched_anchor_idx = cbm_find_matching_anchor(safe_uri, safe_lbl, gate->project_id, anchors, anchor_count);
                    if (matched_anchor_idx < 0) {
                        /* Skip unanchored node */
                        continue;
                    }
                }

                char props_buf[512];
                if (matched_anchor_idx >= 0 && relocs[matched_anchor_idx].was_relocated) {
                    snprintf(props_buf, sizeof(props_buf),
                             "{\"byte_start\":%u,\"byte_len\":%u}",
                             relocs[matched_anchor_idx].new_byte_start,
                             relocs[matched_anchor_idx].new_byte_len);
                } else {
                    snprintf(props_buf, sizeof(props_buf), "%s", code ? code : "{}");
                }

                sqlite3_stmt *ins = NULL;
                const char *ins1 = "INSERT OR REPLACE INTO nodes (cbm_uri, label) VALUES (?, ?)";
                int step_rc = SQLITE_ERROR;
                if (sqlite3_prepare_v2(gate->base_db, ins1, -1, &ins, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(ins, 1, safe_uri, -1, SQLITE_STATIC);
                    sqlite3_bind_text(ins, 2, safe_lbl[0] ? safe_lbl : "", -1, SQLITE_STATIC);
                    step_rc = sqlite3_step(ins);
                    if (step_rc != SQLITE_DONE) {
                        char safe_err[512] = {0};
                        snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                        sqlite3_finalize(ins);
                        sqlite3_finalize(stmt);
                        sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                        sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                        if (out_error && err_sz > 0) {
                            snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed inserting node %s (sqlite error %d: %s)",
                                     safe_uri, step_rc, safe_err);
                        }
                        if (admitted_uris) free(admitted_uris);
                        if (relocs != stack_relocs) free(relocs);
                        return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                    }
                    sqlite3_finalize(ins);
                } else {
                    const char *ins2 = "INSERT OR REPLACE INTO nodes (project, label, name, qualified_name, properties) VALUES (?, ?, ?, ?, ?)";
                    if (sqlite3_prepare_v2(gate->base_db, ins2, -1, &ins, NULL) == SQLITE_OK) {
                        const char *hash_pos = strchr(safe_uri, '#');
                        const char *sym_name = (hash_pos && *(hash_pos + 1)) ? hash_pos + 1 : safe_uri;
                        sqlite3_bind_text(ins, 1, gate->project_id[0] ? gate->project_id : "default", -1, SQLITE_STATIC);
                        sqlite3_bind_text(ins, 2, safe_lbl[0] ? safe_lbl : "Symbol", -1, SQLITE_STATIC);
                        sqlite3_bind_text(ins, 3, sym_name, -1, SQLITE_STATIC);
                        sqlite3_bind_text(ins, 4, safe_uri, -1, SQLITE_STATIC);
                        sqlite3_bind_text(ins, 5, props_buf, -1, SQLITE_TRANSIENT);
                        step_rc = sqlite3_step(ins);
                        if (step_rc != SQLITE_DONE) {
                            char safe_err[512] = {0};
                            snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                            sqlite3_finalize(ins);
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed inserting node %s (sqlite error %d: %s)",
                                         safe_uri, step_rc, safe_err);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        sqlite3_finalize(ins);
                    } else {
                        char safe_err[512] = {0};
                        snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                        sqlite3_finalize(stmt);
                        sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                        sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                        if (out_error && err_sz > 0) {
                            snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed preparing node insert query for %s: %s",
                                     safe_uri, safe_err);
                        }
                        if (admitted_uris) free(admitted_uris);
                        if (relocs != stack_relocs) free(relocs);
                        return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                    }
                }

                if (matched_anchor_idx >= 0 && relocs[matched_anchor_idx].was_relocated) {
                    const char *match_col = base_has_cbm_uri ? "cbm_uri" : (base_has_qualified_name ? "qualified_name" : NULL);
                    if (base_has_byte_start && base_has_byte_len) {
                        if (!match_col) {
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: base schema has no key column (cbm_uri/qualified_name) for %s", safe_uri);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        char upd_sql[256];
                        snprintf(upd_sql, sizeof(upd_sql), "UPDATE nodes SET byte_start = ?, byte_len = ? WHERE %s = ?;", match_col);
                        sqlite3_stmt *coord_stmt = NULL;
                        int prep_coord = sqlite3_prepare_v2(gate->base_db, upd_sql, -1, &coord_stmt, NULL);
                        if (prep_coord != SQLITE_OK) {
                            char safe_err[512] = {0};
                            snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: error preparing coordinate update for %s: %s",
                                         safe_uri, safe_err);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        sqlite3_bind_int(coord_stmt, 1, (int)relocs[matched_anchor_idx].new_byte_start);
                        sqlite3_bind_int(coord_stmt, 2, (int)relocs[matched_anchor_idx].new_byte_len);
                        sqlite3_bind_text(coord_stmt, 3, safe_uri, -1, SQLITE_STATIC);
                        int step_coord = sqlite3_step(coord_stmt);
                        if (step_coord != SQLITE_DONE) {
                            char safe_err[512] = {0};
                            snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                            sqlite3_finalize(coord_stmt);
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed updating relocated coordinates for %s (sqlite error %d: %s)",
                                         safe_uri, step_coord, safe_err);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        sqlite3_finalize(coord_stmt);
                        int changes = sqlite3_changes(gate->base_db);
                        if (changes != 1) {
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: update relocated coordinates for %s affected %d rows (expected 1)",
                                         safe_uri, changes);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        char chk_sql[256];
                        snprintf(chk_sql, sizeof(chk_sql), "SELECT byte_start, byte_len FROM nodes WHERE %s = ? LIMIT 1;", match_col);
                        sqlite3_stmt *chk_coord_stmt = NULL;
                        if (sqlite3_prepare_v2(gate->base_db, chk_sql, -1, &chk_coord_stmt, NULL) != SQLITE_OK) {
                            char safe_err[512] = {0};
                            snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: error preparing coordinate verification query for %s: %s",
                                         safe_uri, safe_err);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        sqlite3_bind_text(chk_coord_stmt, 1, safe_uri, -1, SQLITE_STATIC);
                        int chk_step = sqlite3_step(chk_coord_stmt);
                        if (chk_step != SQLITE_ROW) {
                            char safe_err[512] = {0};
                            snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                            sqlite3_finalize(chk_coord_stmt);
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: coordinate verification read failed for %s: %s", safe_uri, safe_err);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        uint32_t rd_start = (uint32_t)sqlite3_column_int(chk_coord_stmt, 0);
                        uint32_t rd_len = (uint32_t)sqlite3_column_int(chk_coord_stmt, 1);
                        sqlite3_finalize(chk_coord_stmt);
                        if (rd_start != relocs[matched_anchor_idx].new_byte_start || rd_len != relocs[matched_anchor_idx].new_byte_len) {
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: coordinate verification mismatch for %s: expected [%u, %u], found [%u, %u]",
                                         safe_uri, relocs[matched_anchor_idx].new_byte_start, relocs[matched_anchor_idx].new_byte_len,
                                         rd_start, rd_len);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                    } else if (base_has_properties) {
                        if (!match_col) {
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: base schema has no key column (cbm_uri/qualified_name) for %s", safe_uri);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        char upd_sql[256];
                        snprintf(upd_sql, sizeof(upd_sql), "UPDATE nodes SET properties = ? WHERE %s = ?;", match_col);
                        sqlite3_stmt *coord_stmt = NULL;
                        int prep_coord = sqlite3_prepare_v2(gate->base_db, upd_sql, -1, &coord_stmt, NULL);
                        if (prep_coord != SQLITE_OK) {
                            char safe_err[512] = {0};
                            snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: error preparing properties update for %s: %s",
                                         safe_uri, safe_err);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        sqlite3_bind_text(coord_stmt, 1, props_buf, -1, SQLITE_STATIC);
                        sqlite3_bind_text(coord_stmt, 2, safe_uri, -1, SQLITE_STATIC);
                        int step_coord = sqlite3_step(coord_stmt);
                        if (step_coord != SQLITE_DONE) {
                            char safe_err[512] = {0};
                            snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                            sqlite3_finalize(coord_stmt);
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed updating properties for %s (sqlite error %d: %s)",
                                         safe_uri, step_coord, safe_err);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        sqlite3_finalize(coord_stmt);
                        int changes = sqlite3_changes(gate->base_db);
                        if (changes != 1) {
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: update properties for %s affected %d rows (expected 1)",
                                         safe_uri, changes);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        char chk_sql[256];
                        snprintf(chk_sql, sizeof(chk_sql), "SELECT properties FROM nodes WHERE %s = ? LIMIT 1;", match_col);
                        sqlite3_stmt *chk_stmt = NULL;
                        if (sqlite3_prepare_v2(gate->base_db, chk_sql, -1, &chk_stmt, NULL) != SQLITE_OK) {
                            char safe_err[512] = {0};
                            snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: error preparing properties verification query for %s: %s",
                                         safe_uri, safe_err);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        sqlite3_bind_text(chk_stmt, 1, safe_uri, -1, SQLITE_STATIC);
                        int chk_step = sqlite3_step(chk_stmt);
                        if (chk_step != SQLITE_ROW) {
                            char safe_err[512] = {0};
                            snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                            sqlite3_finalize(chk_stmt);
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: properties verification read failed for %s: %s", safe_uri, safe_err);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        const char *rd_props = (const char *)sqlite3_column_text(chk_stmt, 0);
                        int rd_bytes = sqlite3_column_bytes(chk_stmt, 0);
                        char *safe_rd_props = NULL;
                        if (rd_props && rd_bytes >= 0) {
                            safe_rd_props = (char *)malloc((size_t)rd_bytes + 1);
                            if (safe_rd_props) {
                                memcpy(safe_rd_props, rd_props, (size_t)rd_bytes);
                                safe_rd_props[rd_bytes] = '\0';
                            }
                        }
                        sqlite3_finalize(chk_stmt);

                        if (!safe_rd_props) {
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: properties verification read null/oom for %s", safe_uri);
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }

                        /* Parse and structurally verify entire untruncated JSON properties document */
                        bool props_ok = false;
                        yyjson_doc *doc = yyjson_read(safe_rd_props, (size_t)rd_bytes, 0);
                        if (doc) {
                            yyjson_val *root = yyjson_doc_get_root(doc);
                            if (yyjson_is_obj(root)) {
                                yyjson_val *val_start = yyjson_obj_get(root, "byte_start");
                                yyjson_val *val_len = yyjson_obj_get(root, "byte_len");
                                if (val_start && (yyjson_is_uint(val_start) || yyjson_is_int(val_start)) &&
                                    val_len && (yyjson_is_uint(val_len) || yyjson_is_int(val_len))) {
                                    uint64_t num_start = yyjson_get_uint(val_start);
                                    uint64_t num_len = yyjson_get_uint(val_len);
                                    if (num_start == (uint64_t)relocs[matched_anchor_idx].new_byte_start &&
                                        num_len == (uint64_t)relocs[matched_anchor_idx].new_byte_len) {
                                        props_ok = true;
                                    }
                                }
                            }
                            yyjson_doc_free(doc);
                        }

                        if (!props_ok) {
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: properties verification mismatch for %s: expected [%u, %u], found '%.256s'",
                                         safe_uri,
                                         relocs[matched_anchor_idx].new_byte_start,
                                         relocs[matched_anchor_idx].new_byte_len,
                                         safe_rd_props);
                            }
                            free(safe_rd_props);
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        free(safe_rd_props);
                    } else {
                        sqlite3_finalize(stmt);
                        sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                        sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                        if (out_error && err_sz > 0) {
                            snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: base schema has no storage for relocated coordinates of %s",
                                     safe_uri);
                        }
                        if (admitted_uris) free(admitted_uris);
                        if (relocs != stack_relocs) free(relocs);
                        return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                    }
                }

                if (admitted_uris && safe_uri[0] != '\0') {
                    if (admitted_count >= admitted_cap) {
                        size_t new_cap = admitted_cap * 2;
                        char (*new_uris)[CBM_URI_MAX_LEN] = (char (*)[CBM_URI_MAX_LEN])realloc(admitted_uris, new_cap * CBM_URI_MAX_LEN);
                        if (new_uris) {
                            admitted_uris = new_uris;
                            admitted_cap = new_cap;
                        }
                    }
                    if (admitted_count < admitted_cap) {
                        snprintf(admitted_uris[admitted_count], CBM_URI_MAX_LEN, "%s", safe_uri);
                        admitted_count++;
                    }
                }
            }
            if (step_sel_rc != SQLITE_DONE) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_finalize(stmt);
                sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: iteration of symbolic_nodes failed (sqlite error %d: %s)",
                             step_sel_rc, safe_err);
                }
                if (admitted_uris) free(admitted_uris);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }
            sqlite3_finalize(stmt);

            /* Consolidate virtual_edges reading directly from h_db under single transactional snapshot */
            stmt = NULL;
            const char *sel_edges = "SELECT source_uri, target_uri, edge_type, origin_horizon FROM h_db.virtual_edges;";
            prep_rc = sqlite3_prepare_v2(gate->base_db, sel_edges, -1, &stmt, NULL);
            if (prep_rc != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed to prepare virtual_edges query (sqlite error %d: %s)",
                             prep_rc, safe_err);
                }
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }

            step_sel_rc = SQLITE_DONE;
            while ((step_sel_rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                const char *src = (const char *)sqlite3_column_text(stmt, 0);
                const char *tgt = (const char *)sqlite3_column_text(stmt, 1);
                const char *etype = (const char *)sqlite3_column_text(stmt, 2);
                const char *orig = (const char *)sqlite3_column_text(stmt, 3);

                char safe_src[CBM_URI_MAX_LEN] = {0};
                if (src) snprintf(safe_src, sizeof(safe_src), "%s", src);
                char safe_tgt[CBM_URI_MAX_LEN] = {0};
                if (tgt) snprintf(safe_tgt, sizeof(safe_tgt), "%s", tgt);
                char safe_etype[64] = {0};
                if (etype) snprintf(safe_etype, sizeof(safe_etype), "%s", etype);
                char safe_orig[128] = {0};
                if (orig) snprintf(safe_orig, sizeof(safe_orig), "%s", orig);

                bool edge_anchored = true;
                if (anchors && anchor_count > 0) {
                    bool src_ok = false;
                    bool tgt_ok = false;
                    for (size_t i = 0; i < admitted_count; i++) {
                        if (safe_src[0] && strcmp(safe_src, admitted_uris[i]) == 0) src_ok = true;
                        if (safe_tgt[0] && strcmp(safe_tgt, admitted_uris[i]) == 0) tgt_ok = true;
                    }
                    if (!src_ok && safe_src[0]) {
                        char safe_err[512] = {0};
                        int rc_src = cbm_node_exists_in_base(gate->base_db, safe_src, safe_err, sizeof(safe_err));
                        if (rc_src < 0) {
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: error querying base node for edge source %s: %s",
                                         safe_src, safe_err[0] ? safe_err : "query failed");
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        src_ok = (rc_src == 1);
                    }
                    if (!tgt_ok && safe_tgt[0]) {
                        char safe_err[512] = {0};
                        int rc_tgt = cbm_node_exists_in_base(gate->base_db, safe_tgt, safe_err, sizeof(safe_err));
                        if (rc_tgt < 0) {
                            sqlite3_finalize(stmt);
                            sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                            if (out_error && err_sz > 0) {
                                snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: error querying base node for edge target %s: %s",
                                         safe_tgt, safe_err[0] ? safe_err : "query failed");
                            }
                            if (admitted_uris) free(admitted_uris);
                            if (relocs != stack_relocs) free(relocs);
                            return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                        }
                        tgt_ok = (rc_tgt == 1);
                    }

                    edge_anchored = (src_ok && tgt_ok);
                    if (!edge_anchored) {
                        /* Skip virtual edge because at least one endpoint is unanchored or excluded */
                        continue;
                    }
                }

                sqlite3_stmt *ins = NULL;
                const char *ins_edge = "INSERT OR REPLACE INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) VALUES (?, ?, ?, ?, strftime('%s','now'))";
                int step_rc = SQLITE_ERROR;
                if (sqlite3_prepare_v2(gate->base_db, ins_edge, -1, &ins, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(ins, 1, safe_src, -1, SQLITE_STATIC);
                    sqlite3_bind_text(ins, 2, safe_tgt, -1, SQLITE_STATIC);
                    sqlite3_bind_text(ins, 3, safe_etype, -1, SQLITE_STATIC);
                    sqlite3_bind_text(ins, 4, safe_orig[0] ? safe_orig : horizon_id, -1, SQLITE_STATIC);
                    step_rc = sqlite3_step(ins);
                    if (step_rc != SQLITE_DONE) {
                        char safe_err[512] = {0};
                        snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                        sqlite3_finalize(ins);
                        sqlite3_finalize(stmt);
                        sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                        sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                        if (out_error && err_sz > 0) {
                            snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed inserting edge %s -> %s (sqlite error %d: %s)",
                                     safe_src, safe_tgt, step_rc, safe_err);
                        }
                        if (admitted_uris) free(admitted_uris);
                        if (relocs != stack_relocs) free(relocs);
                        return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                    }
                    sqlite3_finalize(ins);
                } else {
                    char safe_err[512] = {0};
                    snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                    sqlite3_finalize(stmt);
                    sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                    sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                    if (out_error && err_sz > 0) {
                        snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: failed preparing edge insert query: %s", safe_err);
                    }
                    if (admitted_uris) free(admitted_uris);
                    if (relocs != stack_relocs) free(relocs);
                    return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
                }
            }
            if (step_sel_rc != SQLITE_DONE) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_finalize(stmt);
                sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "CONSOLIDATION_FAILED: iteration of virtual_edges failed (sqlite error %d: %s)",
                             step_sel_rc, safe_err);
                }
                if (admitted_uris) free(admitted_uris);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_CONSOLIDATION_FAILED;
            }
            sqlite3_finalize(stmt);

            /* Record in generation_log inside transaction so rollback resets it */
            if (sqlite3_exec(gate->base_db, "INSERT OR REPLACE INTO generation_log (generation) VALUES (COALESCE((SELECT MAX(generation) FROM generation_log), 0) + 1);", NULL, NULL, NULL) != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "COMMIT_FAILED: failed to insert into generation_log: %s", safe_err);
                }
                if (admitted_uris) free(admitted_uris);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }

            /* Update horizon status to PROMOTED inside attached horizon db as part of atomic 2PC */
            sqlite3_stmt *st_stmt = NULL;
            const char *upd_h_status =
                "UPDATE h_db.horizon_metadata SET status = 'PROMOTED', last_heartbeat = strftime('%s','now') WHERE horizon_id = ? AND status = 'ACTIVE';";
            if (sqlite3_prepare_v2(gate->base_db, upd_h_status, -1, &st_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(st_stmt, 1, horizon_id, -1, SQLITE_STATIC);
                int step_upd = sqlite3_step(st_stmt);
                if (step_upd != SQLITE_DONE) {
                    char safe_err[512] = {0};
                    snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                    sqlite3_finalize(st_stmt);
                    sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                    sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                    if (out_error && err_sz > 0) {
                        snprintf(out_error, err_sz, "COMMIT_FAILED: failed updating horizon_metadata status to PROMOTED in attached db: %s", safe_err);
                    }
                    if (admitted_uris) free(admitted_uris);
                    if (relocs != stack_relocs) free(relocs);
                    return CBM_ADMISSION_ERR_COMMIT_FAILED;
                }
                sqlite3_finalize(st_stmt);
                if (sqlite3_changes(gate->base_db) != 1) {
                    sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                    sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                    if (out_error && err_sz > 0) {
                        snprintf(out_error, err_sz, "STATE_TRANSITION_FAILED: horizon %s missing or not in ACTIVE state (changes=%d)",
                                 horizon_id, sqlite3_changes(gate->base_db));
                    }
                    if (admitted_uris) free(admitted_uris);
                    if (relocs != stack_relocs) free(relocs);
                    return CBM_ADMISSION_ERR_STATE_TRANSITION_FAILED;
                }
            } else {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "COMMIT_FAILED: failed preparing horizon_metadata update in attached db: %s", safe_err);
                }
                if (admitted_uris) free(admitted_uris);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }

            /* Phase 1: Native Two-Phase Commit on base_db and attached h_db */
            if (sqlite3_exec(gate->base_db, "COMMIT;", NULL, NULL, NULL) != SQLITE_OK) {
                char safe_err[512] = {0};
                snprintf(safe_err, sizeof(safe_err), "%s", sqlite3_errmsg(gate->base_db));
                sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "COMMIT_FAILED: failed to commit transaction across databases: %s", safe_err);
                }
                if (admitted_uris) free(admitted_uris);
                if (relocs != stack_relocs) free(relocs);
                return CBM_ADMISSION_ERR_COMMIT_FAILED;
            }

            /* Detach attached horizon database after successful atomic commit */
            sqlite3_exec(gate->base_db, "DETACH DATABASE h_db;", NULL, NULL, NULL);

            /* On commit success, increment gate->base_generation */
            gate->base_generation++;

            /* Phase 2: Idempotent pool cache sync and reconciliation alert check */
            int prom_rc = cbm_promote_horizon_state(pool, horizon_id);
            if (prom_rc != 0) {
                cbm_log(CBM_LOG_WARN, "admission.reconciliation_alert",
                        "horizon_id", horizon_id,
                        "base_generation", (int64_t)gate->base_generation,
                        "warning", "horizon_db state update returned non-zero after commit",
                        NULL);
            }

            if (admitted_uris) free(admitted_uris);
        }
    }

    if (relocs != stack_relocs) free(relocs);
    return CBM_ADMISSION_OK;
}

int cbm_enforce_union_session(const char *session_id, char *out_err, size_t err_sz) {
    if (!session_id || session_id[0] == '\0') {
        const char *legacy = getenv("CBM_ALLOW_LEGACY_PROMOTION");
        if (legacy && strcmp(legacy, "1") == 0) {
            cbm_log(CBM_LOG_WARN, "union.promotion_legacy_override", "horizon_id", "", "warning", "legacy_promotion_allowed_via_env", NULL);
            return CBM_ADMISSION_OK;
        }
        cbm_refusal_emit(CBM_REFUSAL_SESSION_REQUIRED, "", "promotion blocked: active Union session required");
        if (out_err && err_sz > 0) {
            snprintf(out_err, err_sz, "SESSION_REQUIRED: promotion requires an active Union session");
        }
        return CBM_ADMISSION_ERR_SESSION_REQUIRED;
    }
    return CBM_ADMISSION_OK;
}
