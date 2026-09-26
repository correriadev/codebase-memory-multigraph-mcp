/*
 * union_doc_l0.h — Structural Extraction L0 (Scope C04).
 *
 * Deterministic, syntax-only parsing of markdown documents:
 * - Headings -> section nodes with cbm://docs/<path>#<heading-slug> URIs
 * - FILE_BYTES anchors per section
 * - Frontmatter properties extracted as raw properties
 * - Code block detection
 * - Link extraction into unresolved reference candidates
 *
 * See docs/PRD/novos-paradgimas/specs/doc-plane/SCOPE-C04-structural-extraction-l0.md
 */
#ifndef CBM_UNION_DOC_L0_H
#define CBM_UNION_DOC_L0_H

#include "union_claim.h"
#include "union_anchor.h"
#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_DOC_L0_SECTION_CAP 64
#define CBM_DOC_L0_LINK_CAP 64
#define CBM_DOC_L0_PROP_CAP 16

typedef struct {
    char key[64];
    char value[128];
} CbmDocProperty;

typedef struct {
    char uri[256];        /* cbm://docs/<file_path>#<slug> */
    char heading_title[128];
    char heading_slug[64];
    size_t level;         /* 1 for #, 2 for ##, etc. */
    CbmAnchor anchor;     /* FILE_BYTES anchor */
    char content_snippet[512];
    bool is_verified;     /* derived from anchor status */
    bool is_gone;         /* derived from anchor status */
} CbmDocSectionNode;

typedef struct {
    char source_section_slug[64];
    char target_ref[256]; /* relative path or cbm:// URI */
    bool is_cbm_uri;
} CbmDocUnresolvedLink;

typedef struct {
    char file_path[256];
    CbmDocProperty properties[CBM_DOC_L0_PROP_CAP];
    size_t property_count;

    CbmDocSectionNode sections[CBM_DOC_L0_SECTION_CAP];
    size_t section_count;

    CbmDocUnresolvedLink links[CBM_DOC_L0_LINK_CAP];
    size_t link_count;

    size_t code_block_count;
} CbmDocL0ParseResult;

/* Slug generator helper: text -> lowercased, hyphens, alphanumeric only, ignores numeric/section prefixes */
void cbm_doc_slugify(const char *title, char *out_slug, size_t slug_sz);

/* Parse markdown content into L0 section nodes, anchors, properties and links */
CbmRefusalCode cbm_doc_parse_l0(const char *file_path,
                                const char *markdown_text,
                                CbmDocL0ParseResult *out_result,
                                char *out_reason,
                                size_t reason_sz);

/* Re-check section node status from anchor verification */
void cbm_doc_section_update_status(CbmDocSectionNode *section, const CbmAnchorEnv *env);

#endif /* CBM_UNION_DOC_L0_H */
