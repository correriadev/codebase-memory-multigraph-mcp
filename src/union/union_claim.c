/*
 * union_claim.c — Claim Node Anatomy & Validation (Scope C01).
 */
#include "union_claim.h"
#include "../foundation/log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CBM_BLACKLIST_CAP 32

static const char *k_default_blacklist[] = {
    "as discussed above",
    "the above",
    "aforementioned",
    "as previously",
    "como discutido acima"
};
#define K_DEFAULT_BLACKLIST_COUNT (sizeof(k_default_blacklist) / sizeof(k_default_blacklist[0]))

static char s_custom_blacklist[CBM_BLACKLIST_CAP][64];
static size_t s_custom_blacklist_count = 0;

static const char *k_claim_type_names[] = {
    "DECISION",
    "OPEN_QUESTION",
    "CONSTRAINT",
    "FACT",
    "VERDICT"
};

const char *cbm_claim_type_string(CbmClaimType type) {
    if (type >= 0 && type < CBM_CLAIM_TYPE_COUNT) {
        return k_claim_type_names[type];
    }
    return "UNKNOWN";
}

CbmClaimType cbm_claim_type_from_string(const char *str) {
    if (!str) return CBM_CLAIM_DECISION;
    for (size_t i = 0; i < CBM_CLAIM_TYPE_COUNT; i++) {
        if (strcmp(k_claim_type_names[i], str) == 0) {
            return (CbmClaimType)i;
        }
    }
    return CBM_CLAIM_DECISION;
}

const char *cbm_anchor_kind_string(CbmAnchorKind kind) {
    switch (kind) {
        case CBM_ANCHOR_FILE_BYTES: return "FILE_BYTES";
        case CBM_ANCHOR_LOG_REF: return "LOG_REF";
        case CBM_ANCHOR_DERIVATION_REF: return "DERIVATION_REF";
        default: return "NONE";
    }
}

void cbm_claim_blacklist_reset(void) {
    s_custom_blacklist_count = 0;
}

bool cbm_claim_blacklist_add(const char *phrase) {
    if (!phrase || !phrase[0]) return false;
    if (s_custom_blacklist_count >= CBM_BLACKLIST_CAP) return false;
    strncpy(s_custom_blacklist[s_custom_blacklist_count], phrase,
            sizeof(s_custom_blacklist[0]) - 1);
    s_custom_blacklist[s_custom_blacklist_count][sizeof(s_custom_blacklist[0]) - 1] = '\0';
    s_custom_blacklist_count++;
    return true;
}

/* Portable case-insensitive phrase search with word boundaries */
static const char *case_insensitive_match_phrase(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    if (!needle[0]) return haystack;

    size_t needle_len = strlen(needle);
    for (const char *h = haystack; *h; h++) {
        /* Check word boundary at start */
        if (h > haystack && isalnum((unsigned char)*(h - 1))) {
            continue;
        }

        size_t i = 0;
        while (h[i] && needle[i] &&
               tolower((unsigned char)h[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == needle_len) {
            /* Check word boundary at end */
            if (isalnum((unsigned char)h[i])) {
                continue;
            }
            return h;
        }
    }
    return NULL;
}

bool cbm_claim_is_deictic(const char *predicate, const char **out_matched_phrase) {
    if (!predicate) return false;

    /* Check defaults */
    for (size_t i = 0; i < K_DEFAULT_BLACKLIST_COUNT; i++) {
        if (case_insensitive_match_phrase(predicate, k_default_blacklist[i])) {
            if (out_matched_phrase) *out_matched_phrase = k_default_blacklist[i];
            return true;
        }
    }

    /* Check custom extensions */
    for (size_t i = 0; i < s_custom_blacklist_count; i++) {
        if (case_insensitive_match_phrase(predicate, s_custom_blacklist[i])) {
            if (out_matched_phrase) *out_matched_phrase = s_custom_blacklist[i];
            return true;
        }
    }

    return false;
}

CbmRefusalCode cbm_claim_validate(const CbmClaim *claim, char *out_reason, size_t reason_sz) {
    if (!claim) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "claim pointer is null");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    /* 1. Mandatory structural fields */
    if (!claim->id[0]) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "missing mandatory field: id");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    if (!claim->predicate[0]) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "missing mandatory field: predicate");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    if (!claim->provenance.origin_session[0]) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "missing mandatory field: origin_session");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    if (!claim->provenance.origin_horizon[0]) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "missing mandatory field: origin_horizon");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    if (!claim->provenance.proposed_by[0]) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "missing mandatory field: proposed_by");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    if (!claim->based_on_seq[0]) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "missing mandatory field: based_on_seq");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    if (claim->anchor.kind == CBM_ANCHOR_NONE) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "missing mandatory field: anchor");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    /* 2. Type-specific mandatory fields */
    if (claim->type == CBM_CLAIM_OPEN_QUESTION) {
        if (!claim->consequence[0]) {
            if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "missing mandatory field: consequence");
            return CBM_REFUSAL_CLAIM_INVALID;
        }
    } else if (claim->type == CBM_CLAIM_VERDICT) {
        if (claim->evidence_count == 0) {
            if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "missing mandatory field: evidence");
            return CBM_REFUSAL_CLAIM_INVALID;
        }
    }

    /* 3. One-sentence rule for predicate */
    size_t pred_len = strlen(claim->predicate);
    if (pred_len > CBM_CLAIM_PREDICATE_MAX) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "predicate exceeds %d chars", CBM_CLAIM_PREDICATE_MAX);
        return CBM_REFUSAL_PREDICATE_NOT_SELF_CONTAINED;
    }

    /* No newlines */
    for (size_t i = 0; i < pred_len; i++) {
        if (claim->predicate[i] == '\n' || claim->predicate[i] == '\r') {
            if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "newline in predicate");
            return CBM_REFUSAL_PREDICATE_NOT_SELF_CONTAINED;
        }
    }

    /* Must end with single sentence terminator */
    char last_char = claim->predicate[pred_len - 1];
    if (last_char != '.' && last_char != '!' && last_char != '?') {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "missing sentence terminator at end of predicate");
        return CBM_REFUSAL_PREDICATE_NOT_SELF_CONTAINED;
    }

    /* No internal sentence terminators followed by space */
    for (size_t i = 0; i < pred_len - 1; i++) {
        char c = claim->predicate[i];
        if (c == '.' || c == '!' || c == '?') {
            if (i + 1 < pred_len && isspace((unsigned char)claim->predicate[i + 1])) {
                if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "predicate must be a single sentence");
                return CBM_REFUSAL_PREDICATE_NOT_SELF_CONTAINED;
            }
        }
    }

    /* 4. Deictic blacklist check */
    const char *matched_phrase = NULL;
    if (cbm_claim_is_deictic(claim->predicate, &matched_phrase)) {
        if (out_reason && reason_sz > 0) {
            snprintf(out_reason, reason_sz, "predicate contains blacklisted deictic phrase: '%s'",
                     matched_phrase ? matched_phrase : "");
        }
        return CBM_REFUSAL_PREDICATE_NOT_SELF_CONTAINED;
    }

    return CBM_REFUSAL_OK;
}

void cbm_claim_store_init(CbmClaimStore *store, const char *horizon_id) {
    if (!store) return;
    memset(store, 0, sizeof(*store));
    if (horizon_id) {
        strncpy(store->horizon_id, horizon_id, sizeof(store->horizon_id) - 1);
    }
}

CbmRefusalCode cbm_claim_submit_to_horizon(CbmClaimStore *store,
                                          const CbmClaim *claim,
                                          char *out_reason,
                                          size_t reason_sz) {
    if (!store || !claim) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "null store or claim");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    CbmRefusalCode code = cbm_claim_validate(claim, out_reason, reason_sz);
    if (code != CBM_REFUSAL_OK) {
        cbm_refusal_emit(code, store->horizon_id, out_reason ? out_reason : "");
        return code;
    }

    /* Check if already present, update; else append */
    for (size_t i = 0; i < store->count; i++) {
        if (strcmp(store->claims[i].id, claim->id) == 0) {
            store->claims[i] = *claim;
            store->claims[i].status = EPISTEMIC_PROPOSED;
            return CBM_REFUSAL_OK;
        }
    }

    if (store->count < CBM_CLAIM_STORE_CAP) {
        store->claims[store->count] = *claim;
        store->claims[store->count].status = EPISTEMIC_PROPOSED;
        store->count++;
        return CBM_REFUSAL_OK;
    }

    if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "claim store full");
    return CBM_REFUSAL_BUDGET_EXHAUSTED;
}

const CbmClaim *cbm_claim_store_get(const CbmClaimStore *store, const char *claim_id) {
    if (!store || !claim_id) return NULL;
    for (size_t i = 0; i < store->count; i++) {
        if (strcmp(store->claims[i].id, claim_id) == 0) {
            return &store->claims[i];
        }
    }
    return NULL;
}
