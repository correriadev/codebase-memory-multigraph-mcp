/*
 * union_routing.c — Epistemic Routing & Provenance Transparency (Scope D03).
 */
#include "union_routing.h"
#include "../foundation/log.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool str_contains_case_insensitive(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    size_t needle_len = strlen(needle);
    size_t haystack_len = strlen(haystack);
    if (needle_len > haystack_len) return false;

    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        size_t j = 0;
        while (j < needle_len && tolower((unsigned char)haystack[i + j]) == tolower((unsigned char)needle[j])) {
            j++;
        }
        if (j == needle_len) return true;
    }
    return false;
}

CbmActivityClass cbm_classify_activity(const char *intent_or_prompt) {
    if (!intent_or_prompt) return CBM_ACTIVITY_CONSULTATIVE;

    /* Specialty action triggers */
    static const char *specialty_triggers[] = {
        "implement", "create", "build", "refactor", "generate",
        "scaffold", "write code", "construir", "criar", "implementar",
        "refatorar", "pattern", "clean architecture"
    };
    size_t num_specialty = sizeof(specialty_triggers) / sizeof(specialty_triggers[0]);

    for (size_t i = 0; i < num_specialty; i++) {
        if (str_contains_case_insensitive(intent_or_prompt, specialty_triggers[i])) {
            return CBM_ACTIVITY_SPECIALTY;
        }
    }

    /* Consultative triggers */
    static const char *consultative_triggers[] = {
        "explain", "what does", "show", "describe", "find",
        "how is", "where is", "trace", "como funciona", "explique",
        "listar", "open questions"
    };
    size_t num_consultative = sizeof(consultative_triggers) / sizeof(consultative_triggers[0]);

    for (size_t i = 0; i < num_consultative; i++) {
        if (str_contains_case_insensitive(intent_or_prompt, consultative_triggers[i])) {
            return CBM_ACTIVITY_CONSULTATIVE;
        }
    }

    /* Conservative default for craft assistant */
    return CBM_ACTIVITY_CONSULTATIVE;
}

CbmRefusalCode cbm_validate_specialty_provenance(const CbmSpecialtyJudgment *judgment,
                                                const CbmThemeRegistry *reg,
                                                char *err_reason,
                                                size_t err_len) {
    if (!judgment) {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "null judgment");
        return CBM_REFUSAL_PROVENANCE_UNDECLARED;
    }

    if (judgment->kind == CBM_PROVENANCE_NONE) {
        if (err_reason && err_len > 0) {
            snprintf(err_reason, err_len, "specialty judgment carries undeclared provenance; silent invention is refused");
        }
        cbm_refusal_emit(CBM_REFUSAL_PROVENANCE_UNDECLARED, "routing", "silent invention refused");
        return CBM_REFUSAL_PROVENANCE_UNDECLARED;
    }

    if (judgment->kind == CBM_PROVENANCE_CANON_CITATION) {
        if (judgment->citation.theme_id[0] == '\0') {
            if (err_reason && err_len > 0) snprintf(err_reason, err_len, "canon citation missing theme_id");
            return CBM_REFUSAL_ANCHOR_NOT_FOUND;
        }
        if (judgment->citation.node_uri[0] == '\0' ||
            judgment->citation.pinned_version[0] == '\0') {
            if (err_reason && err_len > 0) {
                snprintf(err_reason, err_len, "canon citation requires node_uri and an exact pinned_version");
            }
            return CBM_REFUSAL_ANCHOR_NOT_FOUND;
        }

        if (reg) {
            CbmThemeEntry entry;
            CbmRefusalCode code = cbm_theme_registry_lookup_version(
                reg, judgment->citation.theme_id, judgment->citation.pinned_version, &entry);
            if (code != CBM_REFUSAL_OK) {
                if (err_reason && err_len > 0) {
                    snprintf(err_reason, err_len,
                             code == CBM_REFUSAL_THEME_PERSISTENCE_FAILED
                                 ? "thematic catalog is unavailable"
                                 : "canon citation theme version not found in registry");
                }
                if (code == CBM_REFUSAL_THEME_PERSISTENCE_FAILED) {
                    return code;
                }
                cbm_refusal_emit(CBM_REFUSAL_ANCHOR_NOT_FOUND, "routing", "citation theme not found");
                return CBM_REFUSAL_ANCHOR_NOT_FOUND;
            }
            if (entry.status == CBM_THEME_ABSENT) {
                if (err_reason && err_len > 0) snprintf(err_reason, err_len, "cited thematic graph is ABSENT");
                return CBM_REFUSAL_ANCHOR_NOT_FOUND;
            }
        }
        return CBM_REFUSAL_OK;
    }

    if (judgment->kind == CBM_PROVENANCE_DECLARED_INVENTION) {
        if (!judgment->invention.declared || judgment->invention.rationale[0] == '\0') {
            if (err_reason && err_len > 0) {
                snprintf(err_reason, err_len, "declared invention missing rationale");
            }
            cbm_refusal_emit(CBM_REFUSAL_PROVENANCE_UNDECLARED, "routing", "declared invention missing rationale");
            return CBM_REFUSAL_PROVENANCE_UNDECLARED;
        }
        return CBM_REFUSAL_OK;
    }

    return CBM_REFUSAL_PROVENANCE_UNDECLARED;
}
