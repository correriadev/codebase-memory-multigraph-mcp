/*
 * union_routing.h — Epistemic Routing & Provenance Transparency (Scope D03).
 *
 * See docs/PRD/novos-paradgimas/specs/knowledge-base/SCOPE-D03-routing-provenance-transparency.md
 */
#ifndef CBM_UNION_ROUTING_H
#define CBM_UNION_ROUTING_H

#include "union_refusal.h"
#include "union_theme_registry.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    CBM_ACTIVITY_CONSULTATIVE = 0,
    CBM_ACTIVITY_SPECIALTY = 1
} CbmActivityClass;

typedef enum {
    CBM_PROVENANCE_NONE = 0,
    CBM_PROVENANCE_CANON_CITATION = 1,
    CBM_PROVENANCE_DECLARED_INVENTION = 2
} CbmProvenanceKind;

typedef struct {
    char theme_id[CBM_THEME_ID_MAX];
    char node_uri[CBM_THEME_URI_MAX];
    char pinned_version[CBM_THEME_VER_MAX];
} CbmCanonCitation;

typedef struct {
    bool declared;
    char rationale[256];
} CbmDeclaredInvention;

typedef struct {
    CbmProvenanceKind kind;
    CbmCanonCitation citation;
    CbmDeclaredInvention invention;
} CbmSpecialtyJudgment;

CbmActivityClass cbm_classify_activity(const char *intent_or_prompt);

CbmRefusalCode cbm_validate_specialty_provenance(const CbmSpecialtyJudgment *judgment,
                                                const CbmThemeRegistry *reg,
                                                char *err_reason,
                                                size_t err_len);

#endif /* CBM_UNION_ROUTING_H */
