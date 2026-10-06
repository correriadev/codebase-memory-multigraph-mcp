#include "admission_gate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    ActiveHorizonLiveness liveness[128];
    size_t active_count = cbm_horizon_pool_get_active_alive(pool, gate->project_id[0] ? gate->project_id : "default", liveness, 128);

    bool semantic_found = false;

    TwoTierAnchor *local_anchors = NULL;
    size_t local_count = 0;
    if (!anchors || anchor_count == 0) {
        sqlite3 *cur_db = NULL;
        if (cbm_horizon_pool_get(pool, current_horizon_id, &cur_db) == 0 && cur_db) {
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(cur_db, "SELECT cbm_uri FROM symbolic_nodes", -1, &stmt, NULL) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    local_count++;
                }
                sqlite3_finalize(stmt);
            }
            if (local_count > 0) {
                local_anchors = (TwoTierAnchor *)calloc(local_count, sizeof(TwoTierAnchor));
                if (sqlite3_prepare_v2(cur_db, "SELECT cbm_uri FROM symbolic_nodes", -1, &stmt, NULL) == SQLITE_OK) {
                    size_t idx = 0;
                    while (sqlite3_step(stmt) == SQLITE_ROW && idx < local_count) {
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
                    sqlite3_finalize(stmt);
                }
            }
        }
        anchors = local_anchors;
        anchor_count = local_count;
    }

    for (size_t i = 0; i < active_count; i++) {
        if (strcmp(liveness[i].horizon_id, current_horizon_id) == 0) continue;

        sqlite3 *sib_db = NULL;
        if (cbm_horizon_pool_get(pool, liveness[i].horizon_id, &sib_db) != 0 || !sib_db) continue;

        // Check physical collision with query pushdown
        sqlite3_stmt *stmt = NULL;
        const char *q_phys = "SELECT cbm_uri FROM symbolic_nodes WHERE cbm_uri LIKE ? LIMIT 1";
        if (sqlite3_prepare_v2(sib_db, q_phys, -1, &stmt, NULL) == SQLITE_OK) {
            for (size_t a = 0; a < anchor_count; a++) {
                const char *a_file = anchors[a].file_path;
                const char *a_sym = anchors[a].symbol_name;
                if (!a_file) continue;

                char pattern[1024];
                snprintf(pattern, sizeof(pattern), "%%%s%%", a_file);
                sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", liveness[i].horizon_id);
                    snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "%s", a_file);
                    snprintf(out_report->conflicting_symbol, sizeof(out_report->conflicting_symbol), "%s", a_sym ? a_sym : "");
                    out_report->is_semantic_only = false;
                    sqlite3_finalize(stmt);
                    if (local_anchors) free(local_anchors);
                    return CBM_ADMISSION_ERR_CONCURRENT_CONFLICT;
                }
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);
        }

        // Check semantic collision with query pushdown
        if (!semantic_found) {
            const char *q_sem = "SELECT source_uri, target_uri FROM virtual_edges WHERE (source_uri LIKE ? AND source_uri LIKE ?) OR (target_uri LIKE ? AND target_uri LIKE ?) LIMIT 1";
            if (sqlite3_prepare_v2(sib_db, q_sem, -1, &stmt, NULL) == SQLITE_OK) {
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
                    if (sqlite3_step(stmt) == SQLITE_ROW) {
                        snprintf(out_report->conflicting_horizon, sizeof(out_report->conflicting_horizon), "%s", liveness[i].horizon_id);
                        snprintf(out_report->conflicting_file, sizeof(out_report->conflicting_file), "%s", a_file);
                        snprintf(out_report->conflicting_symbol, sizeof(out_report->conflicting_symbol), "%s", a_sym);
                        out_report->is_semantic_only = true;
                        semantic_found = true;
                        break;
                    }
                    sqlite3_reset(stmt);
                }
                sqlite3_finalize(stmt);
            }
        }
    }

    if (local_anchors) free(local_anchors);

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

    /* Validate each anchor against the codebase files */
    if (anchors && anchor_count > 0) {
        for (size_t i = 0; i < anchor_count; i++) {
            bool ok = false;
            int rc = cbm_verify_two_tier_anchor(repo_root, &anchors[i], &ok);
            if (rc != 0 || !ok) {
                if (out_error && err_sz > 0) {
                    snprintf(out_error, err_sz, "ANCHOR_DRIFT: anchor '%s' in '%s' diverged from filesystem",
                             anchors[i].symbol_name, anchors[i].file_path);
                }
                return CBM_ADMISSION_ERR_ANCHOR_DRIFT;
            }
        }
    }

    /* Anchors verified! Transition horizon status to PROMOTED */
    if (pool) {
        /* Consolidate promoted nodes and virtual edges into the persistent base graph. */
        if (gate->base_db) {
            sqlite3 *hdb = NULL;
            if (cbm_horizon_pool_get(pool, horizon_id, &hdb) == 0 && hdb) {
                if (sqlite3_exec(gate->base_db, "BEGIN IMMEDIATE;", NULL, NULL, NULL) != SQLITE_OK) {
                    if (out_error && err_sz > 0) snprintf(out_error, err_sz, "CONCURRENT_CONFLICT: could not acquire exclusive lock");
                    return CBM_ADMISSION_ERR_CONCURRENT_CONFLICT;
                }
                
                /* TOCTOU Protection: Verify base_generation hasn't drifted while checking anchors */
                bool generation_drifted = false;
                sqlite3_stmt *gen_stmt = NULL;
                sqlite3_exec(gate->base_db, "CREATE TABLE IF NOT EXISTS generation_log (generation INTEGER PRIMARY KEY);", NULL, NULL, NULL);
                if (sqlite3_prepare_v2(gate->base_db, "SELECT MAX(generation) FROM generation_log", -1, &gen_stmt, NULL) == SQLITE_OK) {
                    if (sqlite3_step(gen_stmt) == SQLITE_ROW && sqlite3_column_type(gen_stmt, 0) != SQLITE_NULL) {
                        uint64_t current_gen = (uint64_t)sqlite3_column_int64(gen_stmt, 0);
                        if (current_gen > gate->base_generation) {
                            generation_drifted = true;
                        }
                    }
                    sqlite3_finalize(gen_stmt);
                }
                if (generation_drifted) {
                    sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                    if (out_error && err_sz > 0) snprintf(out_error, err_sz, "STALE_BASE: base_generation drifted during anchor verification (TOCTOU generation_check)");
                    return CBM_ADMISSION_ERR_CONCURRENT_CONFLICT;
                }
                
                int tx_rc = SQLITE_OK;
                sqlite3_stmt *p_stmt = NULL;
                if (gate->project_id[0]) {
                    if (sqlite3_prepare_v2(gate->base_db, "INSERT OR IGNORE INTO projects (name, root_dir) VALUES (?, '');", -1, &p_stmt, NULL) == SQLITE_OK) {
                        sqlite3_bind_text(p_stmt, 1, gate->project_id, -1, SQLITE_STATIC);
                        if (sqlite3_step(p_stmt) != SQLITE_DONE) tx_rc = SQLITE_ERROR;
                        sqlite3_finalize(p_stmt);
                    } else {
                        tx_rc = SQLITE_ERROR;
                    }
                } else {
                    tx_rc |= sqlite3_exec(gate->base_db, "INSERT OR IGNORE INTO projects (name, root_dir) VALUES ('default', '');", NULL, NULL, NULL);
                }
                tx_rc |= sqlite3_exec(gate->base_db, "CREATE TABLE IF NOT EXISTS virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL);

                if (tx_rc != SQLITE_OK) {
                    sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                    return CBM_ADMISSION_ERR_INVALID_STATE;
                }

                /* Consolidate symbolic_nodes */
                sqlite3_stmt *stmt = NULL;
                const char *sel_nodes = "SELECT cbm_uri, label, code_snippet FROM symbolic_nodes WHERE epistemic_status != 'CONTESTED'";
                if (sqlite3_prepare_v2(hdb, sel_nodes, -1, &stmt, NULL) == SQLITE_OK) {
                    while (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *uri = (const char *)sqlite3_column_text(stmt, 0);
                        const char *lbl = (const char *)sqlite3_column_text(stmt, 1);
                        const char *code = (const char *)sqlite3_column_text(stmt, 2);

                        sqlite3_stmt *ins = NULL;
                        const char *ins1 = "INSERT OR REPLACE INTO nodes (cbm_uri, label) VALUES (?, ?)";
                        if (sqlite3_prepare_v2(gate->base_db, ins1, -1, &ins, NULL) == SQLITE_OK) {
                            sqlite3_bind_text(ins, 1, uri, -1, SQLITE_STATIC);
                            sqlite3_bind_text(ins, 2, lbl ? lbl : "", -1, SQLITE_STATIC);
                            sqlite3_step(ins);
                            sqlite3_finalize(ins);
                        } else {
                            const char *ins2 = "INSERT OR REPLACE INTO nodes (project, label, name, qualified_name, properties) VALUES (?, ?, ?, ?, ?)";
                            if (sqlite3_prepare_v2(gate->base_db, ins2, -1, &ins, NULL) == SQLITE_OK) {
                                const char *hash_pos = uri ? strchr(uri, '#') : NULL;
                                const char *sym_name = (hash_pos && *(hash_pos + 1)) ? hash_pos + 1 : (uri ? uri : "");
                                sqlite3_bind_text(ins, 1, gate->project_id[0] ? gate->project_id : "default", -1, SQLITE_STATIC);
                                sqlite3_bind_text(ins, 2, lbl ? lbl : "Symbol", -1, SQLITE_STATIC);
                                sqlite3_bind_text(ins, 3, sym_name, -1, SQLITE_STATIC);
                                sqlite3_bind_text(ins, 4, uri, -1, SQLITE_STATIC);
                                sqlite3_bind_text(ins, 5, code ? code : "{}", -1, SQLITE_STATIC);
                                sqlite3_step(ins);
                                sqlite3_finalize(ins);
                            }
                        }
                    }
                    sqlite3_finalize(stmt);
                }

                /* Consolidate virtual_edges */
                const char *sel_edges = "SELECT source_uri, target_uri, edge_type, origin_horizon FROM virtual_edges";
                if (sqlite3_prepare_v2(hdb, sel_edges, -1, &stmt, NULL) == SQLITE_OK) {
                    while (sqlite3_step(stmt) == SQLITE_ROW) {
                        const char *src = (const char *)sqlite3_column_text(stmt, 0);
                        const char *tgt = (const char *)sqlite3_column_text(stmt, 1);
                        const char *etype = (const char *)sqlite3_column_text(stmt, 2);
                        const char *orig = (const char *)sqlite3_column_text(stmt, 3);

                        sqlite3_stmt *ins = NULL;
                        const char *ins_edge = "INSERT OR REPLACE INTO virtual_edges (source_uri, target_uri, edge_type, origin_horizon, created_at) VALUES (?, ?, ?, ?, strftime('%s','now'))";
                        if (sqlite3_prepare_v2(gate->base_db, ins_edge, -1, &ins, NULL) == SQLITE_OK) {
                            sqlite3_bind_text(ins, 1, src, -1, SQLITE_STATIC);
                            sqlite3_bind_text(ins, 2, tgt, -1, SQLITE_STATIC);
                            sqlite3_bind_text(ins, 3, etype, -1, SQLITE_STATIC);
                            sqlite3_bind_text(ins, 4, orig ? orig : horizon_id, -1, SQLITE_STATIC);
                            sqlite3_step(ins);
                            sqlite3_finalize(ins);
                        }
                    }
                    sqlite3_finalize(stmt);
                }

                int prom_rc = cbm_promote_horizon_state(pool, horizon_id);
                if (prom_rc != 0) {
                    sqlite3_exec(gate->base_db, "ROLLBACK;", NULL, NULL, NULL);
                    if (out_error && err_sz > 0) {
                        snprintf(out_error, err_sz, "Failed to transition horizon state to PROMOTED");
                    }
                    return CBM_ADMISSION_ERR_INVALID_STATE;
                }

                sqlite3_exec(gate->base_db, "INSERT OR REPLACE INTO generation_log (generation) VALUES (COALESCE((SELECT MAX(generation) FROM generation_log), 0) + 1);", NULL, NULL, NULL);
                gate->base_generation++;

                sqlite3_exec(gate->base_db, "COMMIT;", NULL, NULL, NULL);
            }
        }
    }

    return CBM_ADMISSION_OK;
}
