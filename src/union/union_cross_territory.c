/*
 * union_cross_territory.c — Cross-Territory Typed References & Non-Write Barrier (Scope D04).
 */
#include "union_cross_territory.h"
#include "../foundation/log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

CbmRefusalCode cbm_cross_edge_validate(const CbmCrossEdge *edge,
                                      char *err_reason,
                                      size_t err_len) {
    if (!edge) {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "null edge");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    if (edge->type != CBM_CROSS_EDGE_CONFORMS_TO &&
        edge->type != CBM_CROSS_EDGE_DEVIATES_FROM &&
        edge->type != CBM_CROSS_EDGE_CONSULTS) {
        if (err_reason && err_len > 0) {
            snprintf(err_reason, err_len, "invalid cross-territory edge type; must be CONFORMS_TO, DEVIATES_FROM, or CONSULTS");
        }
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_cross_territory_check_write(CbmCallerContext ctx,
                                              const char *target_uri,
                                              char *err_reason,
                                              size_t err_len) {
    if (!target_uri) return CBM_REFUSAL_OK;

    bool is_theme_uri = (strncmp(target_uri, "theme://", 8) == 0);

    if (is_theme_uri && ctx == CBM_CONTEXT_PROJECT_SESSION) {
        if (err_reason && err_len > 0) {
            snprintf(err_reason, err_len, "thematic graphs are read-only to project sessions; write forbidden");
        }
        cbm_refusal_emit(CBM_REFUSAL_TERRITORY_WRITE_FORBIDDEN, "cross_territory", "project session write into theme forbidden");
        return CBM_REFUSAL_TERRITORY_WRITE_FORBIDDEN;
    }

    return CBM_REFUSAL_OK;
}

bool cbm_detect_cross_territory_conflict(const char *project_claim_uri,
                                         const char *theme_rule_uri,
                                         CbmContestationEvent *out_event) {
    if (!project_claim_uri || !theme_rule_uri) return false;

    if (out_event) {
        strncpy(out_event->project_claim_uri, project_claim_uri, sizeof(out_event->project_claim_uri) - 1);
        strncpy(out_event->theme_rule_uri, theme_rule_uri, sizeof(out_event->theme_rule_uri) - 1);
        out_event->status = CBM_CONTESTATION_OPEN;
        out_event->created_at = (uint64_t)time(NULL);
    }

    cbm_refusal_emit(CBM_REFUSAL_CONTEST_UNPROVEN, "cross_territory", "open contestation between project claim and theme rule");
    return true;
}
