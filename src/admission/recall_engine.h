#ifndef CBM_RECALL_ENGINE_H
#define CBM_RECALL_ENGINE_H

#include "../core/cbm_uri.h"
#include "../core/visited_set.h"
#include "../core/symbolic_node.h"

#define CBM_RECALL_OK 0
#define CBM_RECALL_MAX_AFFECTED 256
#define CBM_RECALL_ERR_TX_FAILED -10

typedef struct {
    CbmUri contested_root;
    char reason[256];
    char affected_uris[CBM_RECALL_MAX_AFFECTED][CBM_URI_MAX_LEN];
    size_t affected_count;
    size_t total_affected_count;
    bool is_committed;
} RecallReport;

static inline const char *cbm_recall_error_to_string(int err_code) {
    if (err_code == CBM_RECALL_ERR_TX_FAILED) return "TRANSACTION_FAILED";
    return "UNKNOWN";
}

/* Traverses reverse dependencies (deps^-1) up to depth limit (or unbounded if max_depth == 0) with cycle pruning */
int cbm_bfs_reverse_deps_acyclic(sqlite3 *db, const CbmUri *contested_root, VisitedSet *visited, uint32_t max_depth, RecallReport *out_report);

/* Triggers epistemic recall on a contested symbol, updating statuses and generating report */
int cbm_trigger_recall(sqlite3 *db, const CbmUri *contested_root, const char *reason, RecallReport *out_report);

#endif /* CBM_RECALL_ENGINE_H */
