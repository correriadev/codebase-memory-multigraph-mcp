#ifndef CBM_ADMISSION_GATE_H
#define CBM_ADMISSION_GATE_H

#include "../core/horizon_pool.h"
#include "anchor_checker.h"
#include <stdint.h>
#include <stdbool.h>

#define CBM_ADMISSION_OK 0
#define CBM_ADMISSION_ERR_ANCHOR_DRIFT -1
#define CBM_ADMISSION_ERR_HORIZON_NOT_FOUND -2
#define CBM_ADMISSION_ERR_INVALID_STATE -3

typedef struct AdmissionGate {
    char project_id[64];
    uint64_t base_generation;
    uint32_t active_recalls_count;
    sqlite3 *base_db;
} AdmissionGate;

/* Initialize admission gate */
int cbm_admission_gate_init(AdmissionGate *gate, const char *project_id, uint64_t generation);

/* Attach base database handle for consolidation */
void cbm_admission_gate_set_base_db(AdmissionGate *gate, sqlite3 *base_db);

/* Promote horizon to base after validating anchors against filesystem */
int cbm_promote_horizon(AdmissionGate *gate,
                        HorizonConnectionPool *pool,
                        const char *repo_root,
                        const char *horizon_id,
                        const TwoTierAnchor *anchors,
                        size_t anchor_count,
                        char *out_error,
                        size_t err_sz);

#endif /* CBM_ADMISSION_GATE_H */
