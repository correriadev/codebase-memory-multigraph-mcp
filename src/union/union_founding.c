/*
 * union_founding.c — Closure Sweep Founding Proposals (Scope D06).
 */
#include "union_founding.h"
#include "../foundation/log.h"

#include <stdio.h>
#include <string.h>

CbmRefusalCode cbm_founding_adjudicate(CbmThemeRegistry *reg,
                                       const CbmFoundingProposal *proposal,
                                       bool operator_accepted,
                                       bool is_agent_autonomous,
                                       CbmClosureExclusions *out_exclusions) {
    if (!proposal) return CBM_REFUSAL_CLAIM_INVALID;

    if (is_agent_autonomous) {
        cbm_refusal_emit(CBM_REFUSAL_AUTO_FOUNDING_FORBIDDEN, "closure_sweep",
                         "autonomous founding of theme graphs is forbidden; operator validation required");
        return CBM_REFUSAL_AUTO_FOUNDING_FORBIDDEN;
    }

    if (!operator_accepted) {
        if (out_exclusions) {
            out_exclusions->founding_proposals_declined++;
        }
        return CBM_REFUSAL_OK;
    }

    if (reg) {
        CbmThemeEntry entry;
        memset(&entry, 0, sizeof(entry));
        strncpy(entry.theme_id, proposal->suggested_theme_id, sizeof(entry.theme_id) - 1);
        strncpy(entry.name, proposal->suggested_theme_id, sizeof(entry.name) - 1);
        strncpy(entry.namespace, proposal->namespace, sizeof(entry.namespace) - 1);
        strncpy(entry.version, "0.1.0", sizeof(entry.version) - 1);
        strncpy(entry.curator, proposal->suggested_curator[0] ? proposal->suggested_curator : "curator_pending",
                sizeof(entry.curator) - 1);
        entry.status = CBM_THEME_ABSENT; /* seed registered, awaiting graph instantiation */
        strncpy(entry.founding_provenance, proposal->origin_session, sizeof(entry.founding_provenance) - 1);

        return cbm_theme_registry_register(reg, &entry, NULL, 0);
    }

    return CBM_REFUSAL_OK;
}
