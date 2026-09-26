/*
 * union_theme_registry.c — KnowledgeBase Registry Graph & Absence Querying (Scope D01).
 */
#include "union_theme_registry.h"
#include "../foundation/log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

void cbm_theme_registry_init(CbmThemeRegistry *reg) {
    if (!reg) return;
    memset(reg, 0, sizeof(*reg));
}

CbmRefusalCode cbm_theme_registry_register(CbmThemeRegistry *reg,
                                          const CbmThemeEntry *entry,
                                          char *err_reason,
                                          size_t err_len) {
    if (!reg || !entry) {
        if (err_reason && err_len > 0) {
            snprintf(err_reason, err_len, "null registry or entry");
        }
        return CBM_REFUSAL_THEME_SCHEMA_INVALID;
    }

    if (entry->theme_id[0] == '\0') {
        if (err_reason && err_len > 0) {
            snprintf(err_reason, err_len, "missing mandatory field: theme_id");
        }
        cbm_refusal_emit(CBM_REFUSAL_THEME_SCHEMA_INVALID, "theme_registry", "missing mandatory field: theme_id");
        return CBM_REFUSAL_THEME_SCHEMA_INVALID;
    }
    if (entry->namespace[0] == '\0') {
        if (err_reason && err_len > 0) {
            snprintf(err_reason, err_len, "missing mandatory field: namespace");
        }
        cbm_refusal_emit(CBM_REFUSAL_THEME_SCHEMA_INVALID, "theme_registry", "missing mandatory field: namespace");
        return CBM_REFUSAL_THEME_SCHEMA_INVALID;
    }
    if (entry->curator[0] == '\0') {
        if (err_reason && err_len > 0) {
            snprintf(err_reason, err_len, "missing mandatory field: curator");
        }
        cbm_refusal_emit(CBM_REFUSAL_THEME_SCHEMA_INVALID, "theme_registry", "missing mandatory field: curator");
        return CBM_REFUSAL_THEME_SCHEMA_INVALID;
    }
    if (entry->version[0] == '\0') {
        if (err_reason && err_len > 0) {
            snprintf(err_reason, err_len, "missing mandatory field: version");
        }
        cbm_refusal_emit(CBM_REFUSAL_THEME_SCHEMA_INVALID, "theme_registry", "missing mandatory field: version");
        return CBM_REFUSAL_THEME_SCHEMA_INVALID;
    }

    /* Check if already registered (update existing) */
    for (size_t i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i].theme_id, entry->theme_id) == 0) {
            reg->entries[i] = *entry;
            reg->entries[i].updated_at = (uint64_t)time(NULL);
            return CBM_REFUSAL_OK;
        }
    }

    if (reg->count >= CBM_THEME_REGISTRY_CAP) {
        if (err_reason && err_len > 0) {
            snprintf(err_reason, err_len, "theme registry capacity exceeded");
        }
        return CBM_REFUSAL_SCOPE_EXCEEDED;
    }

    reg->entries[reg->count] = *entry;
    reg->entries[reg->count].created_at = (uint64_t)time(NULL);
    reg->entries[reg->count].updated_at = reg->entries[reg->count].created_at;
    reg->count++;

    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_theme_registry_lookup(const CbmThemeRegistry *reg,
                                        const char *theme_id,
                                        CbmThemeEntry *out_entry) {
    if (!reg || !theme_id) return CBM_REFUSAL_THEME_UNKNOWN;

    for (size_t i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i].theme_id, theme_id) == 0) {
            if (out_entry) {
                *out_entry = reg->entries[i];
            }
            return CBM_REFUSAL_OK;
        }
    }

    cbm_refusal_emit(CBM_REFUSAL_THEME_UNKNOWN, "theme_registry", theme_id);
    return CBM_REFUSAL_THEME_UNKNOWN;
}

CbmRefusalCode cbm_theme_registry_query_namespace(const CbmThemeRegistry *reg,
                                                 const char *namespace,
                                                 CbmThemeEntry *out_entries,
                                                 size_t max_entries,
                                                 size_t *out_count) {
    if (!reg || !namespace || !out_count) return CBM_REFUSAL_THEME_UNKNOWN;

    size_t matched = 0;
    for (size_t i = 0; i < reg->count && matched < max_entries; i++) {
        if (strcmp(reg->entries[i].namespace, namespace) == 0) {
            if (out_entries) {
                out_entries[matched] = reg->entries[i];
            }
            matched++;
        }
    }
    *out_count = matched;
    return CBM_REFUSAL_OK;
}
