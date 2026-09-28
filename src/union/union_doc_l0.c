/*
 * union_doc_l0.c — Structural Extraction L0 (Scope C04).
 */
#include "union_doc_l0.h"
#include "../foundation/log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cbm_doc_slugify(const char *title, char *out_slug, size_t slug_sz) {
    if (!out_slug || slug_sz == 0) return;
    out_slug[0] = '\0';
    if (!title) return;

    /* Skip leading symbols, section marks (§), numbers and dots:
     * e.g. "§3.7 Emendation II" -> start at "Emendation II" */
    const unsigned char *p = (const unsigned char *)title;
    while (*p) {
        if (isalpha(*p)) {
            break;
        }
        p++;
    }

    size_t out_idx = 0;
    bool last_was_hyphen = false;

    while (*p && out_idx + 1 < slug_sz) {
        if (isalnum(*p)) {
            out_slug[out_idx++] = (char)tolower(*p);
            last_was_hyphen = false;
        } else if (*p == ' ' || *p == '-' || *p == '_') {
            if (!last_was_hyphen && out_idx > 0) {
                out_slug[out_idx++] = '-';
                last_was_hyphen = true;
            }
        }
        p++;
    }

    /* Strip trailing hyphens */
    while (out_idx > 0 && out_slug[out_idx - 1] == '-') {
        out_idx--;
    }
    out_slug[out_idx] = '\0';
}

static const char *parse_frontmatter(const char *text,
                                    CbmDocL0ParseResult *out_result,
                                    char *out_reason,
                                    size_t reason_sz) {
    if (strncmp(text, "---", 3) != 0) {
        return text; /* No frontmatter */
    }

    /* Skip opening '---' */
    const char *p = text + 3;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\r') p++;
    if (*p == '\n') p++;
    else return text; /* not frontmatter */

    /* Find closing '---' */
    const char *end = strstr(p, "\n---");
    if (!end) {
        if (out_reason && reason_sz > 0) {
            snprintf(out_reason, reason_sz, "unclosed frontmatter block");
        }
        return NULL;
    }

    /* Parse key-value lines */
    const char *cur = p;
    while (cur < end && out_result->property_count < CBM_DOC_L0_PROP_CAP) {
        const char *colon = strchr(cur, ':');
        const char *line_end = strchr(cur, '\n');
        if (!line_end || line_end > end) line_end = end;

        if (colon && colon < line_end) {
            /* Key */
            const char *k_start = cur;
            while (k_start < colon && isspace((unsigned char)*k_start)) k_start++;
            const char *k_end = colon;
            while (k_end > k_start && isspace((unsigned char)*(k_end - 1))) k_end--;

            /* Value */
            const char *v_start = colon + 1;
            while (v_start < line_end && isspace((unsigned char)*v_start)) v_start++;
            const char *v_end = line_end;
            while (v_end > v_start && (isspace((unsigned char)*(v_end - 1)) || *(v_end - 1) == '\r')) v_end--;

            size_t k_len = (size_t)(k_end - k_start);
            size_t v_len = (size_t)(v_end - v_start);

            if (k_len > 0 && k_len < sizeof(out_result->properties[0].key) &&
                v_len < sizeof(out_result->properties[0].value)) {
                CbmDocProperty *prop = &out_result->properties[out_result->property_count++];
                memcpy(prop->key, k_start, k_len);
                prop->key[k_len] = '\0';
                memcpy(prop->value, v_start, v_len);
                prop->value[v_len] = '\0';
            }
        }
        cur = line_end + 1;
    }

    /* Advance past closing '---' */
    const char *after_close = end + 4;
    while (*after_close == ' ' || *after_close == '\t') after_close++;
    if (*after_close == '\r') after_close++;
    if (*after_close == '\n') after_close++;
    return after_close;
}

CbmRefusalCode cbm_doc_parse_l0(const char *file_path,
                                const char *markdown_text,
                                CbmDocL0ParseResult *out_result,
                                char *out_reason,
                                size_t reason_sz) {
    if (!file_path || !markdown_text || !out_result) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "null arguments to doc_parse_l0");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    memset(out_result, 0, sizeof(*out_result));
    strncpy(out_result->file_path, file_path, sizeof(out_result->file_path) - 1);

    /* 1. Parse frontmatter */
    const char *content_start = parse_frontmatter(markdown_text, out_result, out_reason, reason_sz);
    if (!content_start) {
        cbm_refusal_emit(CBM_REFUSAL_CLAIM_INVALID, file_path,
                         out_reason ? out_reason : "frontmatter error");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    /* 2. Line by line parsing for headings, code blocks and links */
    const char *cur = content_start;
    bool in_code_block = false;

    while (*cur) {
        const char *line_start = cur;
        const char *line_end = strchr(cur, '\n');
        if (!line_end) {
            line_end = cur + strlen(cur);
        }

        /* Check code block fences */
        if (strncmp(line_start, "```", 3) == 0) {
            if (!in_code_block) {
                in_code_block = true;
                out_result->code_block_count++;
            } else {
                in_code_block = false;
            }
            cur = *line_end ? line_end + 1 : line_end;
            continue;
        }

        if (!in_code_block) {
            /* Check heading: #... */
            if (*line_start == '#') {
                size_t level = 0;
                const char *h = line_start;
                while (*h == '#' && level < 6) {
                    level++;
                    h++;
                }
                if (*h == ' ') {
                    while (*h == ' ') h++;
                    const char *title_start = h;
                    const char *title_end = line_end;
                    while (title_end > title_start &&
                           (isspace((unsigned char)*(title_end - 1)) || *(title_end - 1) == '\r')) {
                        title_end--;
                    }

                    if (out_result->section_count < CBM_DOC_L0_SECTION_CAP) {
                        CbmDocSectionNode *sec = &out_result->sections[out_result->section_count++];
                        sec->level = level;

                        size_t title_len = (size_t)(title_end - title_start);
                        if (title_len >= sizeof(sec->heading_title)) {
                            title_len = sizeof(sec->heading_title) - 1;
                        }
                        memcpy(sec->heading_title, title_start, title_len);
                        sec->heading_title[title_len] = '\0';

                        char slug[sizeof(sec->heading_slug)];
                        cbm_doc_slugify(sec->heading_title, slug, sizeof(slug));
                        memcpy(sec->heading_slug, slug, sizeof(slug));
                        snprintf(sec->uri, sizeof(sec->uri), "cbm://%s#%s", file_path, slug);

                        /* Configure FILE_BYTES anchor */
                        sec->anchor.kind = CBM_ANCHOR_FILE_BYTES;
                        strncpy(sec->anchor.file_bytes.file_path, file_path,
                                sizeof(sec->anchor.file_bytes.file_path) - 1);
                        sec->anchor.file_bytes.byte_start = (size_t)(line_start - markdown_text);

                        const char *heading_line_end = line_end;
                        if (heading_line_end > line_start && *(heading_line_end - 1) == '\r') {
                            heading_line_end--;
                        }
                        size_t line_len = (size_t)(heading_line_end - line_start);
                        sec->anchor.file_bytes.byte_len = line_len;

                        if (line_len >= sizeof(sec->anchor.file_bytes.expected_text)) {
                            line_len = sizeof(sec->anchor.file_bytes.expected_text) - 1;
                        }
                        memcpy(sec->anchor.file_bytes.expected_text, line_start, line_len);
                        sec->anchor.file_bytes.expected_text[line_len] = '\0';

                        sec->is_verified = false;
                        sec->is_gone = false;
                    }
                }
            }

            /* Extract Markdown links: [text](target) */
            const char *p = line_start;
            while (p < line_end) {
                const char *open_sq = strchr(p, '[');
                if (!open_sq || open_sq >= line_end) break;
                const char *close_sq = strchr(open_sq, ']');
                if (!close_sq || close_sq >= line_end) break;
                if (*(close_sq + 1) == '(') {
                    const char *open_paren = close_sq + 1;
                    const char *close_paren = strchr(open_paren, ')');
                    if (close_paren && close_paren < line_end) {
                        const char *target_start = open_paren + 1;
                        size_t target_len = (size_t)(close_paren - target_start);
                        if (target_len > 0 && target_len < sizeof(out_result->links[0].target_ref) &&
                            out_result->link_count < CBM_DOC_L0_LINK_CAP) {
                            CbmDocUnresolvedLink *link = &out_result->links[out_result->link_count++];
                            memcpy(link->target_ref, target_start, target_len);
                            link->target_ref[target_len] = '\0';
                            link->is_cbm_uri = (strncmp(link->target_ref, "cbm://", 6) == 0);
                        }
                        p = close_paren + 1;
                        continue;
                    }
                }
                p = open_sq + 1;
            }
        }

        cur = *line_end ? line_end + 1 : line_end;
    }

    if (in_code_block) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "unclosed code block at EOF");
        cbm_refusal_emit(CBM_REFUSAL_CLAIM_INVALID, file_path, "unclosed code block");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    return CBM_REFUSAL_OK;
}

void cbm_doc_section_update_status(CbmDocSectionNode *section, const CbmAnchorEnv *env) {
    if (!section) return;

    CbmRefusalCode refusal = CBM_REFUSAL_OK;
    char reason[128] = {0};
    CbmAnchorVerifyStatus status = cbm_anchor_verify(&section->anchor, env, &refusal, reason, sizeof(reason));

    if (status == CBM_ANCHOR_VERIFY_OK) {
        section->is_verified = true;
        section->is_gone = false;
    } else {
        section->is_verified = false;
        section->is_gone = true;
    }
}
