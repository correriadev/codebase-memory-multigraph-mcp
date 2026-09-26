/*
 * union_cross_territory.h — Cross-Territory Typed References & Non-Write Barrier (Scope D04).
 *
 * See docs/PRD/novos-paradgimas/specs/knowledge-base/SCOPE-D04-cross-territory-typed-references.md
 */
#ifndef CBM_UNION_CROSS_TERRITORY_H
#define CBM_UNION_CROSS_TERRITORY_H

#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_CROSS_URI_MAX 256

typedef enum {
    CBM_CROSS_EDGE_CONFORMS_TO = 0,
    CBM_CROSS_EDGE_DEVIATES_FROM = 1,
    CBM_CROSS_EDGE_CONSULTS = 2
} CbmCrossEdgeType;

typedef struct {
    char source_project_node[CBM_CROSS_URI_MAX];
    char target_theme_uri[CBM_CROSS_URI_MAX];
    CbmCrossEdgeType type;
} CbmCrossEdge;

typedef enum {
    CBM_CONTEXT_PROJECT_SESSION = 0,
    CBM_CONTEXT_CURATOR_WORKFLOW = 1
} CbmCallerContext;

typedef enum {
    CBM_CONTESTATION_NONE = 0,
    CBM_CONTESTATION_OPEN = 1,
    CBM_CONTESTATION_RESOLVED = 2
} CbmContestationStatus;

typedef struct {
    char project_claim_uri[CBM_CROSS_URI_MAX];
    char theme_rule_uri[CBM_CROSS_URI_MAX];
    CbmContestationStatus status;
    uint64_t created_at;
} CbmContestationEvent;

CbmRefusalCode cbm_cross_edge_validate(const CbmCrossEdge *edge,
                                      char *err_reason,
                                      size_t err_len);

CbmRefusalCode cbm_cross_territory_check_write(CbmCallerContext ctx,
                                              const char *target_uri,
                                              char *err_reason,
                                              size_t err_len);

bool cbm_detect_cross_territory_conflict(const char *project_claim_uri,
                                         const char *theme_rule_uri,
                                         CbmContestationEvent *out_event);

#endif /* CBM_UNION_CROSS_TERRITORY_H */
