#include "scope_validator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cbm_scope_validation_report_free(ScopeValidationReport *report) {
    if (!report) return;

    if (report->isolated_nodes) {
        for (size_t i = 0; i < report->isolated_nodes_count; i++) {
            if (report->isolated_nodes[i]) {
                free(report->isolated_nodes[i]);
            }
        }
        free(report->isolated_nodes);
        report->isolated_nodes = NULL;
    }
    report->isolated_nodes_count = 0;

    if (report->unresolved_deps) {
        for (size_t i = 0; i < report->unresolved_deps_count; i++) {
            if (report->unresolved_deps[i]) {
                free(report->unresolved_deps[i]);
            }
        }
        free(report->unresolved_deps);
        report->unresolved_deps = NULL;
    }
    report->unresolved_deps_count = 0;
}

static bool append_string(char ***arr, size_t *count, const char *str) {
    if (!arr || !count || !str) return false;
    char **new_arr = (char **)realloc(*arr, (*count + 1) * sizeof(char *));
    if (!new_arr) return false;
    *arr = new_arr;
    char *copy = strdup(str);
    if (!copy) return false;
    (*arr)[*count] = copy;
    (*count)++;
    return true;
}

static sqlite3_stmt *prepare_base_node_lookup(sqlite3 *base_db) {
    if (!base_db) return NULL;

    const char *primary_sql =
        "SELECT 1 FROM nodes WHERE qualified_name = ?1 OR cbm_uri = ?1 OR (name = ?2 AND ?2 != '') LIMIT 1;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(base_db, primary_sql, -1, &stmt, NULL) == SQLITE_OK) {
        return stmt;
    }

    /* Primary query failed (e.g. column mismatch). Inspect schema via PRAGMA table_info */
    bool has_qn = false;
    bool has_uri = false;
    bool has_name = false;

    sqlite3_stmt *info_stmt = NULL;
    if (sqlite3_prepare_v2(base_db, "PRAGMA table_info(nodes);", -1, &info_stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(info_stmt) == SQLITE_ROW) {
            const char *col = (const char *)sqlite3_column_text(info_stmt, 1);
            if (!col) continue;
            if (strcmp(col, "qualified_name") == 0) has_qn = true;
            else if (strcmp(col, "cbm_uri") == 0) has_uri = true;
            else if (strcmp(col, "name") == 0) has_name = true;
        }
        sqlite3_finalize(info_stmt);
    }

    char sql[512] = {0};
    if (has_qn && has_name) {
        snprintf(sql, sizeof(sql),
                 "SELECT 1 FROM nodes WHERE qualified_name = ?1 OR (name = ?2 AND ?2 != '') LIMIT 1;");
    } else if (has_uri && has_name) {
        snprintf(sql, sizeof(sql),
                 "SELECT 1 FROM nodes WHERE cbm_uri = ?1 OR (name = ?2 AND ?2 != '') LIMIT 1;");
    } else if (has_uri) {
        snprintf(sql, sizeof(sql),
                 "SELECT 1 FROM nodes WHERE cbm_uri = ?1 LIMIT 1;");
    } else if (has_qn) {
        snprintf(sql, sizeof(sql),
                 "SELECT 1 FROM nodes WHERE qualified_name = ?1 LIMIT 1;");
    } else if (has_name) {
        snprintf(sql, sizeof(sql),
                 "SELECT 1 FROM nodes WHERE name = ?2 AND ?2 != '' LIMIT 1;");
    } else {
        return NULL;
    }

    if (sqlite3_prepare_v2(base_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return NULL;
    }
    return stmt;
}

int cbm_validate_scope_horizon(sqlite3 *hdb,
                               sqlite3 *base_db,
                               bool strict_connectivity,
                               ScopeValidationReport *report,
                               char *err_buf,
                               size_t err_sz) {
    if (!hdb) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "Invalid horizon database handle");
        }
        return -1;
    }
    if (!report) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "ScopeValidationReport pointer is NULL");
        }
        return -1;
    }

    memset(report, 0, sizeof(*report));

    /* a) Isolated Dangling Nodes (PBI-07) */
    const char *iso_sql =
        "SELECT cbm_uri FROM symbolic_nodes "
        "WHERE is_dangling = 1 AND label != 'Section' "
        "AND cbm_uri NOT IN ("
        "    SELECT source_uri FROM virtual_edges "
        "    UNION "
        "    SELECT target_uri FROM virtual_edges"
        ");";

    sqlite3_stmt *iso_stmt = NULL;
    int rc = sqlite3_prepare_v2(hdb, iso_sql, -1, &iso_stmt, NULL);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(iso_stmt) == SQLITE_ROW) {
            const char *uri = (const char *)sqlite3_column_text(iso_stmt, 0);
            if (uri && strict_connectivity) {
                append_string(&report->isolated_nodes, &report->isolated_nodes_count, uri);
            }
        }
        sqlite3_finalize(iso_stmt);
    } else {
        const char *errmsg = sqlite3_errmsg(hdb);
        if (strstr(errmsg, "no such table") == NULL) {
            if (err_buf && err_sz > 0) {
                snprintf(err_buf, err_sz, "Failed to query isolated nodes: %s", errmsg);
            }
            cbm_scope_validation_report_free(report);
            return -1;
        }
    }

    /* b) Base Adjacency Check (PBI-08) */
    const char *dep_sql =
        "SELECT DISTINCT target_uri FROM virtual_edges "
        "WHERE target_uri NOT IN (SELECT cbm_uri FROM symbolic_nodes);";

    sqlite3_stmt *dep_stmt = NULL;
    rc = sqlite3_prepare_v2(hdb, dep_sql, -1, &dep_stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_stmt *base_stmt = NULL;
        if (base_db) {
            base_stmt = prepare_base_node_lookup(base_db);
        }

        while (sqlite3_step(dep_stmt) == SQLITE_ROW) {
            const char *target_uri = (const char *)sqlite3_column_text(dep_stmt, 0);
            if (!target_uri || !target_uri[0]) continue;

            if (base_db == NULL) {
                /* If base_db is NULL, skip base check */
                continue;
            }

            if (!base_stmt) {
                /* Base DB provided but nodes table or query unavailable: unresolved */
                append_string(&report->unresolved_deps, &report->unresolved_deps_count, target_uri);
                continue;
            }

            const char *h = strchr(target_uri, '#');
            const char *sym = (h && *(h + 1)) ? (h + 1) : "";

            int param_count = sqlite3_bind_parameter_count(base_stmt);
            sqlite3_bind_text(base_stmt, 1, target_uri, -1, SQLITE_STATIC);
            if (param_count >= 2) {
                sqlite3_bind_text(base_stmt, 2, sym, -1, SQLITE_STATIC);
            }

            int step_rc = sqlite3_step(base_stmt);
            if (step_rc != SQLITE_ROW) {
                append_string(&report->unresolved_deps, &report->unresolved_deps_count, target_uri);
            }
            sqlite3_reset(base_stmt);
        }

        if (base_stmt) {
            sqlite3_finalize(base_stmt);
        }
        sqlite3_finalize(dep_stmt);
    } else {
        const char *errmsg = sqlite3_errmsg(hdb);
        if (strstr(errmsg, "no such table") == NULL) {
            if (err_buf && err_sz > 0) {
                snprintf(err_buf, err_sz, "Failed to query external dependencies: %s", errmsg);
            }
            cbm_scope_validation_report_free(report);
            return -1;
        }
    }

    /* c) Return 0 if valid (no isolated and no unresolved deps), non-zero if issues found */
    if (report->isolated_nodes_count > 0 || report->unresolved_deps_count > 0) {
        return 1;
    }
    return 0;
}
