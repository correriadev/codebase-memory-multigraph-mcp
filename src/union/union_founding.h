/*
 * union_founding.h — Closure Sweep Founding Proposals (Scope D06).
 *
 * See docs/PRD/novos-paradgimas/specs/knowledge-base/SCOPE-D06-founding-proposals.md
 */
#ifndef CBM_UNION_FOUNDING_H
#define CBM_UNION_FOUNDING_H

#include "union_refusal.h"
#include "union_theme_registry.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char suggested_theme_id[CBM_THEME_ID_MAX];
    char namespace[CBM_THEME_NS_MAX];
    char rationale[256];
    char origin_session[64];
    char suggested_curator[CBM_THEME_CURATOR_MAX];
    size_t seed_rules_count;
} CbmFoundingProposal;

typedef struct {
    size_t founding_proposals_declined;
    size_t claims_discarded;
} CbmClosureExclusions;

CbmRefusalCode cbm_founding_adjudicate(CbmThemeRegistry *reg,
                                       const CbmFoundingProposal *proposal,
                                       bool operator_accepted,
                                       bool is_agent_autonomous,
                                       CbmClosureExclusions *out_exclusions);

#endif /* CBM_UNION_FOUNDING_H */
