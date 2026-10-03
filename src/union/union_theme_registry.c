/*
 * union_theme_registry.c — KnowledgeBase Registry Graph & Absence Querying (Scope D01).
 */
#include "union_theme_registry.h"
#include "../foundation/log.h"
#include "../foundation/compat_fs.h"
#include "../foundation/compat.h"
#include "../foundation/platform.h"
#include <yyjson/yyjson.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void cbm_theme_registry_init(CbmThemeRegistry *reg) {
    if (!reg) return;
    memset(reg, 0, sizeof(*reg));
    reg->storage_ready = true;
}

static const char *theme_status_name(CbmThemeStatus status) {
    switch (status) {
        case CBM_THEME_DEPRECATED: return "DEPRECATED";
        case CBM_THEME_ABSENT: return "ABSENT";
        default: return "ACTIVE";
    }
}

static bool theme_write_string(yyjson_mut_doc *doc, yyjson_mut_val *obj,
                               const char *key, const char *value) {
    return yyjson_mut_obj_add_strcpy(doc, obj, key, value ? value : "");
}

static bool theme_registry_save(const CbmThemeRegistry *reg) {
    if (!reg || !reg->storage_path[0]) return true;
    char tmp_path[CBM_THEME_STORE_PATH_MAX + 8];
    int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", reg->storage_path);
    if (n < 0 || (size_t)n >= sizeof(tmp_path)) return false;

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) return false;
    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, arr);
    for (size_t i = 0; i < reg->count; i++) {
        const CbmThemeEntry *entry = &reg->entries[i];
        yyjson_mut_val *obj = yyjson_mut_obj(doc);
        bool ok = obj &&
            theme_write_string(doc, obj, "theme_id", entry->theme_id) &&
            theme_write_string(doc, obj, "name", entry->name) &&
            theme_write_string(doc, obj, "namespace", entry->namespace) &&
            theme_write_string(doc, obj, "target_uri", entry->target_uri) &&
            theme_write_string(doc, obj, "target_generation", entry->target_generation) &&
            theme_write_string(doc, obj, "version", entry->version) &&
            theme_write_string(doc, obj, "curator", entry->curator) &&
            theme_write_string(doc, obj, "status", theme_status_name(entry->status)) &&
            theme_write_string(doc, obj, "founding_provenance", entry->founding_provenance) &&
            theme_write_string(doc, obj, "description", entry->description) &&
            theme_write_string(doc, obj, "aliases", entry->aliases) &&
            theme_write_string(doc, obj, "tags", entry->tags) &&
            yyjson_mut_obj_add_uint(doc, obj, "created_at", entry->created_at) &&
            yyjson_mut_obj_add_uint(doc, obj, "updated_at", entry->updated_at) &&
            yyjson_mut_arr_add_val(arr, obj);
        if (!ok) {
            yyjson_mut_doc_free(doc);
            return false;
        }
    }
    bool ok = yyjson_mut_write_file(tmp_path, doc, 0, NULL, NULL);
    yyjson_mut_doc_free(doc);
    if (!ok) return false;
    if (cbm_rename_replace(tmp_path, reg->storage_path) != 0) {
        cbm_unlink(tmp_path);
        return false;
    }
    return true;
}

static void theme_copy_json_string(char *dst, size_t dst_size,
                                   yyjson_val *obj, const char *key) {
    yyjson_val *value = obj ? yyjson_obj_get(obj, key) : NULL;
    if (!dst || !dst_size || !value || !yyjson_is_str(value)) return;
    snprintf(dst, dst_size, "%s", yyjson_get_str(value));
}

bool cbm_theme_registry_open(CbmThemeRegistry *reg, const char *path) {
    if (!reg || !path || !path[0] || strlen(path) >= sizeof(reg->storage_path)) return false;
    snprintf(reg->storage_path, sizeof(reg->storage_path), "%s", path);
    reg->storage_ready = false;
    if (!cbm_file_exists(path)) {
        reg->storage_ready = true;
        return true;
    }

    yyjson_doc *doc = yyjson_read_file(path, 0, NULL, NULL);
    if (!doc) return false;
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_arr(root) || yyjson_arr_size(root) > CBM_THEME_REGISTRY_CAP) {
        yyjson_doc_free(doc);
        return false;
    }
    CbmThemeRegistry loaded;
    cbm_theme_registry_init(&loaded);
    snprintf(loaded.storage_path, sizeof(loaded.storage_path), "%s", path);
    size_t idx, max;
    yyjson_val *value;
    yyjson_arr_foreach(root, idx, max, value) {
        CbmThemeEntry *entry = &loaded.entries[loaded.count];
        theme_copy_json_string(entry->theme_id, sizeof(entry->theme_id), value, "theme_id");
        theme_copy_json_string(entry->name, sizeof(entry->name), value, "name");
        theme_copy_json_string(entry->namespace, sizeof(entry->namespace), value, "namespace");
        theme_copy_json_string(entry->target_uri, sizeof(entry->target_uri), value, "target_uri");
        theme_copy_json_string(entry->target_generation, sizeof(entry->target_generation), value, "target_generation");
        theme_copy_json_string(entry->version, sizeof(entry->version), value, "version");
        theme_copy_json_string(entry->curator, sizeof(entry->curator), value, "curator");
        theme_copy_json_string(entry->founding_provenance, sizeof(entry->founding_provenance), value, "founding_provenance");
        theme_copy_json_string(entry->description, sizeof(entry->description), value, "description");
        theme_copy_json_string(entry->aliases, sizeof(entry->aliases), value, "aliases");
        theme_copy_json_string(entry->tags, sizeof(entry->tags), value, "tags");
        char status[24] = {0};
        theme_copy_json_string(status, sizeof(status), value, "status");
        entry->status = strcmp(status, "ABSENT") == 0 ? CBM_THEME_ABSENT :
                        strcmp(status, "DEPRECATED") == 0 ? CBM_THEME_DEPRECATED : CBM_THEME_ACTIVE;
        yyjson_val *created = yyjson_obj_get(value, "created_at");
        yyjson_val *updated = yyjson_obj_get(value, "updated_at");
        if (created && yyjson_is_uint(created)) entry->created_at = yyjson_get_uint(created);
        if (updated && yyjson_is_uint(updated)) entry->updated_at = yyjson_get_uint(updated);
        if (!entry->theme_id[0] || !entry->namespace[0] || !entry->version[0]) {
            yyjson_doc_free(doc);
            return false;
        }
        loaded.count++;
    }
    *reg = loaded;
    reg->storage_ready = true;
    yyjson_doc_free(doc);
    return true;
}

static bool theme_contains_ci(const char *haystack, const char *needle) {
    if (!haystack || !needle || !needle[0]) return false;
    size_t n = strlen(needle), h = strlen(haystack);
    if (n > h) return false;
    for (size_t i = 0; i <= h - n; i++) {
        size_t j = 0;
        while (j < n && tolower((unsigned char)haystack[i + j]) ==
                         tolower((unsigned char)needle[j])) j++;
        if (j == n) return true;
    }
    return false;
}

static unsigned theme_match_score(const CbmThemeEntry *entry, const char *query) {
    if (!query || !query[0]) return 1;
    const char *fields[] = {entry->theme_id, entry->name, entry->namespace,
                            entry->description, entry->aliases, entry->tags};
    unsigned score = 0;
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        if (theme_contains_ci(fields[i], query)) score += (i == 0 || i == 1) ? 8 : 4;
    }
    /* Treat whitespace-separated words as an AND query across metadata. */
    char terms[512];
    snprintf(terms, sizeof(terms), "%s", query);
    for (char *p = terms; *p; p++) if (*p == ',' || *p == ';') *p = ' ';
    char haystack[CBM_THEME_ID_MAX + CBM_THEME_NAME_MAX + CBM_THEME_NS_MAX +
                  CBM_THEME_DESCRIPTION_MAX + CBM_THEME_ALIASES_MAX + CBM_THEME_TAGS_MAX + 8];
    snprintf(haystack, sizeof(haystack), "%s %s %s %s %s %s", entry->theme_id,
             entry->name, entry->namespace, entry->description, entry->aliases, entry->tags);
    unsigned terms_found = 0, terms_total = 0;
    char *save = NULL;
    for (char *tok = strtok_r(terms, " \t\r\n", &save); tok;
         tok = strtok_r(NULL, " \t\r\n", &save)) {
        terms_total++;
        if (theme_contains_ci(haystack, tok)) terms_found++;
    }
    if (!terms_total || terms_found != terms_total) return 0;
    return score + terms_found;
}

CbmRefusalCode cbm_theme_registry_search(const CbmThemeRegistry *reg,
                                         const char *query,
                                         const char *namespace_filter,
                                         const char *status_filter,
                                         size_t offset,
                                         size_t limit,
                                         CbmThemeSearchHit *out_hits,
                                         size_t out_capacity,
                                         size_t *out_total,
                                         size_t *out_count) {
    if (!reg || !out_total || !out_count) return CBM_REFUSAL_THEME_SCHEMA_INVALID;
    if (!reg->storage_ready) return CBM_REFUSAL_THEME_PERSISTENCE_FAILED;
    CbmThemeSearchHit matches[CBM_THEME_REGISTRY_CAP];
    size_t count = 0;
    for (size_t i = 0; i < reg->count; i++) {
        const CbmThemeEntry *entry = &reg->entries[i];
        if (namespace_filter && namespace_filter[0] && strcmp(entry->namespace, namespace_filter) != 0) continue;
        if (status_filter && status_filter[0] && strcmp(theme_status_name(entry->status), status_filter) != 0) continue;
        unsigned score = theme_match_score(entry, query);
        if (!score) continue;
        matches[count].entry = *entry;
        matches[count].score = score;
        count++;
    }
    for (size_t i = 1; i < count; i++) {
        CbmThemeSearchHit value = matches[i];
        size_t j = i;
        while (j > 0 && (matches[j - 1].score < value.score ||
               (matches[j - 1].score == value.score &&
                strcmp(matches[j - 1].entry.theme_id, value.entry.theme_id) > 0))) {
            matches[j] = matches[j - 1];
            j--;
        }
        matches[j] = value;
    }
    *out_total = count;
    *out_count = 0;
    if (!out_hits || !limit || offset >= count) return CBM_REFUSAL_OK;
    size_t end = offset + limit;
    if (end > count) end = count;
    if (end - offset > out_capacity) end = offset + out_capacity;
    for (size_t i = offset; i < end; i++) out_hits[(*out_count)++] = matches[i];
    return CBM_REFUSAL_OK;
}

static CbmRefusalCode theme_registry_register_snapshot(CbmThemeRegistry *reg,
                                                      const CbmThemeEntry *entry,
                                                      char *err_reason,
                                                      size_t err_len) {
    if (!reg || !entry) {
        if (err_reason && err_len > 0) {
            snprintf(err_reason, err_len, "null registry or entry");
        }
        return CBM_REFUSAL_THEME_SCHEMA_INVALID;
    }
    if (!reg->storage_ready) {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "theme catalog storage is unavailable");
        return CBM_REFUSAL_THEME_PERSISTENCE_FAILED;
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

    /* A published theme version is immutable. Only its lifecycle status may
     * change in place; changing its content requires a new version ID. */
    for (size_t i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i].theme_id, entry->theme_id) == 0 &&
            strcmp(reg->entries[i].version, entry->version) == 0) {
            CbmThemeEntry previous = reg->entries[i];
            const CbmThemeEntry *old = &reg->entries[i];
            if (strcmp(old->name, entry->name) != 0 ||
                strcmp(old->namespace, entry->namespace) != 0 ||
                strcmp(old->target_uri, entry->target_uri) != 0 ||
                strcmp(old->target_generation, entry->target_generation) != 0 ||
                strcmp(old->curator, entry->curator) != 0 ||
                strcmp(old->founding_provenance, entry->founding_provenance) != 0 ||
                strcmp(old->description, entry->description) != 0 ||
                strcmp(old->aliases, entry->aliases) != 0 ||
                strcmp(old->tags, entry->tags) != 0) {
                if (err_reason && err_len > 0) snprintf(err_reason, err_len, "published theme version is immutable; register a new version");
                return CBM_REFUSAL_THEME_VERSION_IMMUTABLE;
            }
            reg->entries[i].status = entry->status;
            reg->entries[i].updated_at = (uint64_t)time(NULL);
            if (!theme_registry_save(reg)) {
                reg->entries[i] = previous;
                if (err_reason && err_len > 0) snprintf(err_reason, err_len, "theme catalog persistence failed");
                return CBM_REFUSAL_THEME_PERSISTENCE_FAILED;
            }
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

    if (!theme_registry_save(reg)) {
        reg->count--;
        memset(&reg->entries[reg->count], 0, sizeof(reg->entries[reg->count]));
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "theme catalog persistence failed");
        return CBM_REFUSAL_THEME_PERSISTENCE_FAILED;
    }

    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_theme_registry_register(CbmThemeRegistry *reg,
                                          const CbmThemeEntry *entry,
                                          char *err_reason,
                                          size_t err_len) {
    if (!reg || !entry) return theme_registry_register_snapshot(reg, entry, err_reason, err_len);
    if (!reg->storage_path[0]) {
        return theme_registry_register_snapshot(reg, entry, err_reason, err_len);
    }

    char lock_path[CBM_THEME_STORE_PATH_MAX + 8];
    int lock_len = snprintf(lock_path, sizeof(lock_path), "%s.lock", reg->storage_path);
    if (lock_len < 0 || (size_t)lock_len >= sizeof(lock_path) || cbm_mkdir(lock_path) != 0) {
        if (err_reason && err_len) snprintf(err_reason, err_len, "theme catalog is being updated by another writer");
        return CBM_REFUSAL_THEME_PERSISTENCE_FAILED;
    }

    /* Refresh the on-disk snapshot before mutation so a long-lived server
     * instance cannot silently replace entries written by another instance. */
    CbmThemeRegistry latest;
    cbm_theme_registry_init(&latest);
    if (!cbm_theme_registry_open(&latest, reg->storage_path)) {
        (void)cbm_rmdir(lock_path);
        if (err_reason && err_len) snprintf(err_reason, err_len, "theme catalog could not be refreshed");
        return CBM_REFUSAL_THEME_PERSISTENCE_FAILED;
    }
    CbmRefusalCode code = theme_registry_register_snapshot(&latest, entry, err_reason, err_len);
    if (code == CBM_REFUSAL_OK) *reg = latest;
    (void)cbm_rmdir(lock_path);
    return code;
}

CbmRefusalCode cbm_theme_registry_lookup(const CbmThemeRegistry *reg,
                                        const char *theme_id,
                                        CbmThemeEntry *out_entry) {
    if (!reg || !theme_id) return CBM_REFUSAL_THEME_UNKNOWN;
    if (!reg->storage_ready) return CBM_REFUSAL_THEME_PERSISTENCE_FAILED;

    const CbmThemeEntry *latest = NULL;
    for (size_t i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i].theme_id, theme_id) == 0) latest = &reg->entries[i];
    }
    if (latest) {
        if (out_entry) *out_entry = *latest;
        return CBM_REFUSAL_OK;
    }

    cbm_refusal_emit(CBM_REFUSAL_THEME_UNKNOWN, "theme_registry", theme_id);
    return CBM_REFUSAL_THEME_UNKNOWN;
}

CbmRefusalCode cbm_theme_registry_lookup_version(const CbmThemeRegistry *reg,
                                                 const char *theme_id,
                                                 const char *version,
                                                 CbmThemeEntry *out_entry) {
    if (!reg || !theme_id || !version) return CBM_REFUSAL_THEME_UNKNOWN;
    if (!reg->storage_ready) return CBM_REFUSAL_THEME_PERSISTENCE_FAILED;
    for (size_t i = 0; i < reg->count; i++) {
        if (strcmp(reg->entries[i].theme_id, theme_id) == 0 &&
            strcmp(reg->entries[i].version, version) == 0) {
            if (out_entry) *out_entry = reg->entries[i];
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
    if (!reg->storage_ready) return CBM_REFUSAL_THEME_PERSISTENCE_FAILED;

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
