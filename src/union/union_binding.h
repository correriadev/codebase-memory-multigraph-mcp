/*
 * union_binding.h — Binding Claims (DEVE/PODE) & Deviation Ledger (Scope D02).
 *
 * See docs/PRD/novos-paradgimas/specs/knowledge-base/SCOPE-D02-binding-claims.md
 */
#ifndef CBM_UNION_BINDING_H
#define CBM_UNION_BINDING_H

#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_BINDING_ID_MAX 64
#define CBM_BINDING_THEME_ID_MAX 64
#define CBM_BINDING_VER_MAX 32
#define CBM_BINDING_SCOPE_MAX 128
#define CBM_BINDING_PRINCIPAL_MAX 64
#define CBM_BINDING_RULE_REF_MAX 128
#define CBM_BINDING_REASON_MAX 256
#define CBM_BINDING_CAP 64
#define CBM_DEVIATION_CAP 64
#define CBM_BINDING_STORE_PATH_MAX 1024

typedef enum {
    CBM_BINDING_NORMATIVE = 0, /* DEVE */
    CBM_BINDING_CONSULTED = 1  /* PODE */
} CbmBindingMode;

typedef enum {
    CBM_BINDING_ACTIVE = 0,
    CBM_BINDING_DRIFT_PENDING = 1,
    CBM_BINDING_ORPHAN = 2
} CbmBindingStatus;

typedef struct {
    char claim_id[CBM_BINDING_ID_MAX];
    char theme_id[CBM_BINDING_THEME_ID_MAX];
    char pinned_version[CBM_BINDING_VER_MAX];
    CbmBindingMode mode;
    char binding_scope[CBM_BINDING_SCOPE_MAX];
    char validated_by[CBM_BINDING_PRINCIPAL_MAX];
    CbmBindingStatus status;
    uint64_t created_at;
    uint64_t updated_at;
} CbmBindingClaim;

typedef enum {
    CBM_DEVIATION_ACTIVE = 0,
    CBM_DEVIATION_RECONCILED = 1
} CbmDeviationStatus;

typedef struct {
    char claim_id[CBM_BINDING_ID_MAX];
    char theme_id[CBM_BINDING_THEME_ID_MAX];
    char theme_rule_node_ref[CBM_BINDING_RULE_REF_MAX];
    char reason[CBM_BINDING_REASON_MAX];
    char affected_scope[CBM_BINDING_SCOPE_MAX];
    char validated_by[CBM_BINDING_PRINCIPAL_MAX];
    CbmDeviationStatus status;
    uint64_t created_at;
} CbmDeviationClaim;

typedef struct {
    CbmBindingClaim bindings[CBM_BINDING_CAP];
    size_t binding_count;
    CbmDeviationClaim deviations[CBM_DEVIATION_CAP];
    size_t deviation_count;
    char storage_path[CBM_BINDING_STORE_PATH_MAX];
    bool storage_ready;
} CbmBindingLedger;

void cbm_binding_ledger_init(CbmBindingLedger *ledger);
bool cbm_binding_ledger_open(CbmBindingLedger *ledger, const char *path);

CbmRefusalCode cbm_binding_validate_and_admit(CbmBindingLedger *ledger,
                                              const CbmBindingClaim *binding,
                                              char *err_reason,
                                              size_t err_len);

CbmRefusalCode cbm_binding_lookup(const CbmBindingLedger *ledger,
                                 const char *theme_id,
                                 CbmBindingClaim *out_binding);

CbmRefusalCode cbm_deviation_admit(CbmBindingLedger *ledger,
                                   const CbmDeviationClaim *deviation,
                                   char *err_reason,
                                   size_t err_len);

CbmRefusalCode cbm_deviation_query_active(const CbmBindingLedger *ledger,
                                          const char *theme_id,
                                          CbmDeviationClaim *out_devs,
                                          size_t max_devs,
                                          size_t *out_count);

#endif /* CBM_UNION_BINDING_H */
