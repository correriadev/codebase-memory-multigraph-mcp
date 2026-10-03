/*
 * union_theme_registry.h — KnowledgeBase Registry Graph & Absence Querying (Scope D01).
 *
 * See docs/PRD/novos-paradgimas/specs/knowledge-base/SCOPE-D01-registry-graph.md
 */
#ifndef CBM_UNION_THEME_REGISTRY_H
#define CBM_UNION_THEME_REGISTRY_H

#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_THEME_ID_MAX 64
#define CBM_THEME_NAME_MAX 128
#define CBM_THEME_NS_MAX 32
#define CBM_THEME_URI_MAX 256
#define CBM_THEME_VER_MAX 32
#define CBM_THEME_CURATOR_MAX 64
#define CBM_THEME_PROV_MAX 64
#define CBM_THEME_DESCRIPTION_MAX 512
#define CBM_THEME_ALIASES_MAX 256
#define CBM_THEME_TAGS_MAX 256
#define CBM_THEME_STORE_PATH_MAX 1024
#define CBM_THEME_GENERATION_MAX 64
#define CBM_THEME_REGISTRY_CAP 128

typedef enum {
    CBM_THEME_ACTIVE = 0,
    CBM_THEME_DEPRECATED = 1,
    CBM_THEME_ABSENT = 2
} CbmThemeStatus;

typedef struct {
    char theme_id[CBM_THEME_ID_MAX];
    char name[CBM_THEME_NAME_MAX];
    char namespace[CBM_THEME_NS_MAX];
    char target_uri[CBM_THEME_URI_MAX];
    char target_generation[CBM_THEME_GENERATION_MAX];
    char version[CBM_THEME_VER_MAX];
    char curator[CBM_THEME_CURATOR_MAX];
    CbmThemeStatus status;
    char founding_provenance[CBM_THEME_PROV_MAX];
    char description[CBM_THEME_DESCRIPTION_MAX];
    char aliases[CBM_THEME_ALIASES_MAX];
    char tags[CBM_THEME_TAGS_MAX];
    uint64_t created_at;
    uint64_t updated_at;
} CbmThemeEntry;

typedef struct {
    CbmThemeEntry entries[CBM_THEME_REGISTRY_CAP];
    size_t count;
    char storage_path[CBM_THEME_STORE_PATH_MAX];
    bool storage_ready;
} CbmThemeRegistry;

typedef struct {
    CbmThemeEntry entry;
    unsigned score;
} CbmThemeSearchHit;

void cbm_theme_registry_init(CbmThemeRegistry *reg);

/* Attach the durable catalog used by MCP servers. Missing files create an
 * empty catalog; malformed files fail closed and leave the registry empty. */
bool cbm_theme_registry_open(CbmThemeRegistry *reg, const char *path);

/* Search catalog metadata. Empty query matches every entry; namespace/status
 * are exact filters. Results are ranked and paged, with total before paging. */
CbmRefusalCode cbm_theme_registry_search(const CbmThemeRegistry *reg,
                                         const char *query,
                                         const char *namespace_filter,
                                         const char *status_filter,
                                         size_t offset,
                                         size_t limit,
                                         CbmThemeSearchHit *out_hits,
                                         size_t out_capacity,
                                         size_t *out_total,
                                         size_t *out_count);

CbmRefusalCode cbm_theme_registry_register(CbmThemeRegistry *reg,
                                          const CbmThemeEntry *entry,
                                          char *err_reason,
                                          size_t err_len);

CbmRefusalCode cbm_theme_registry_lookup(const CbmThemeRegistry *reg,
                                        const char *theme_id,
                                        CbmThemeEntry *out_entry);

CbmRefusalCode cbm_theme_registry_lookup_version(const CbmThemeRegistry *reg,
                                                 const char *theme_id,
                                                 const char *version,
                                                 CbmThemeEntry *out_entry);

CbmRefusalCode cbm_theme_registry_query_namespace(const CbmThemeRegistry *reg,
                                                 const char *namespace,
                                                 CbmThemeEntry *out_entries,
                                                 size_t max_entries,
                                                 size_t *out_count);

#endif /* CBM_UNION_THEME_REGISTRY_H */
