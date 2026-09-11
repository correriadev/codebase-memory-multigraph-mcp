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
        int rc = cbm_promote_horizon_state(pool, horizon_id);
        if (rc != 0) {
            if (out_error && err_sz > 0) {
                snprintf(out_error, err_sz, "Failed to transition horizon state to PROMOTED");
            }
            return CBM_ADMISSION_ERR_INVALID_STATE;
        }

        /* Consolidate promoted nodes and virtual edges into base graph if available */
        if (gate && gate->base_db) {
            sqlite3 *hdb = NULL;
            if (cbm_horizon_pool_get(pool, horizon_id, &hdb) == 0 && hdb) {
                sqlite3_exec(gate->base_db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
                sqlite3_exec(gate->base_db, "INSERT OR IGNORE INTO projects (name, root_dir) VALUES ('default', '');", NULL, NULL, NULL);
                if (gate->project_id[0]) {
                    char psql[256];
                    snprintf(psql, sizeof(psql), "INSERT OR IGNORE INTO projects (name, root_dir) VALUES ('%s', '');", gate->project_id);
                    sqlite3_exec(gate->base_db, psql, NULL, NULL, NULL);
                }
                sqlite3_exec(gate->base_db, "CREATE TABLE IF NOT EXISTS virtual_edges (source_uri TEXT, target_uri TEXT, edge_type TEXT, origin_horizon TEXT, created_at INTEGER, PRIMARY KEY(source_uri, target_uri, edge_type));", NULL, NULL, NULL);

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
                                sqlite3_bind_text(ins, 1, gate->project_id[0] ? gate->project_id : "default", -1, SQLITE_STATIC);
                                sqlite3_bind_text(ins, 2, lbl ? lbl : "Symbol", -1, SQLITE_STATIC);
                                sqlite3_bind_text(ins, 3, uri, -1, SQLITE_STATIC);
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

                sqlite3_exec(gate->base_db, "COMMIT;", NULL, NULL, NULL);
            }
        }
    }

    return CBM_ADMISSION_OK;
}
