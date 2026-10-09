#include "anchor_checker.h"
#include "../core/cbm_uri.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const TSLanguage *tree_sitter_c(void);
const TSLanguage *tree_sitter_typescript(void);
const TSLanguage *tree_sitter_python(void);

#ifdef _WIN32
#define strncasecmp _strnicmp
#else
#include <strings.h>
#endif

static bool is_path_sep(char c) {
    return c == '/' || c == '\\';
}

static bool has_dot_dot_traversal(const char *path) {
    if (!path) return false;
    for (const char *p = path; *p; p++) {
        if (p[0] == '.' && p[1] == '.') {
            bool prev_is_sep = (p == path) || is_path_sep(*(p - 1));
            bool next_is_sep = (p[2] == '\0') || is_path_sep(p[2]);
            if (prev_is_sep && next_is_sep) {
                return true;
            }
        }
    }
    return false;
}

static bool is_path_absolute(const char *path) {
    if (!path || !path[0]) return false;
    if (path[0] == '/' || path[0] == '\\') return true;
    if (((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z')) && path[1] == ':') {
        return true;
    }
    return false;
}

static bool make_file_path(const char *root, const char *rel, char *out, size_t out_sz) {
    if (!rel || !rel[0] || !out || out_sz == 0) {
        if (out && out_sz > 0) out[0] = '\0';
        return false;
    }

    /* Reject any relative path traversal sequence (..) */
    if (has_dot_dot_traversal(rel)) {
        out[0] = '\0';
        return false;
    }

    if (!root || !root[0]) {
        out[0] = '\0';
        return false;
    }

    char norm_root[1024];
    snprintf(norm_root, sizeof(norm_root), "%s", root);
    for (char *p = norm_root; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    size_t rlen = strlen(norm_root);
    while (rlen > 1 && norm_root[rlen - 1] == '/') {
        norm_root[rlen - 1] = '\0';
        rlen--;
    }

    char norm_rel[1024];
    snprintf(norm_rel, sizeof(norm_rel), "%s", rel);
    for (char *p = norm_rel; *p; p++) {
        if (*p == '\\') *p = '/';
    }

    if (is_path_absolute(norm_rel)) {
        bool match = false;
#if defined(_WIN32)
        if (_strnicmp(norm_rel, norm_root, rlen) == 0) {
            if (norm_rel[rlen] == '/' || norm_rel[rlen] == '\0') {
                match = true;
            }
        }
#else
        if (strncmp(norm_rel, norm_root, rlen) == 0) {
            if (norm_rel[rlen] == '/' || norm_rel[rlen] == '\0') {
                match = true;
            }
        }
#endif
        if (!match) {
            out[0] = '\0';
            return false;
        }
        if (snprintf(out, out_sz, "%s", norm_rel) >= (int)out_sz) {
            out[0] = '\0';
            return false;
        }
    } else {
        const char *rel_start = norm_rel;
        while (rel_start[0] == '.' && rel_start[1] == '/') {
            rel_start += 2;
        }
        while (rel_start[0] == '/') {
            rel_start++;
        }
        if (!rel_start[0]) {
            out[0] = '\0';
            return false;
        }
        if (snprintf(out, out_sz, "%s/%s", norm_root, rel_start) >= (int)out_sz) {
            out[0] = '\0';
            return false;
        }
    }

#if !defined(_WIN32)
    if (((out[0] >= 'a' && out[0] <= 'z') || (out[0] >= 'A' && out[0] <= 'Z')) && out[1] == ':' && out[2] == '/') {
        char drive = out[0];
        if (drive >= 'A' && drive <= 'Z') drive += ('a' - 'A');
        char temp[1024];
        snprintf(temp, sizeof(temp), "/mnt/%c/%s", drive, out + 3);
        snprintf(out, out_sz, "%s", temp);
    }
#endif

    return true;
}


CbmSupportedLanguage cbm_resolve_language_from_path(const char *path) {
    if (!path || !path[0]) return CBM_LANG_UNKNOWN;

    /* Find the last file component (after '/' or '\\') */
    const char *slash = strrchr(path, '/');
    const char *bslash = strrchr(path, '\\');
    const char *fname = path;
    if (slash && slash >= fname) fname = slash + 1;
    if (bslash && bslash >= fname) fname = bslash + 1;

    /* Find the extension */
    const char *dot = strrchr(fname, '.');
    if (!dot) return CBM_LANG_UNKNOWN;

    if (strcmp(dot, ".c") == 0 || strcmp(dot, ".h") == 0) {
        return CBM_LANG_C;
    }
    if (strcmp(dot, ".ts") == 0 || strcmp(dot, ".tsx") == 0 ||
        strcmp(dot, ".js") == 0 || strcmp(dot, ".jsx") == 0) {
        return CBM_LANG_TYPESCRIPT;
    }
    if (strcmp(dot, ".py") == 0) {
        return CBM_LANG_PYTHON;
    }

    return CBM_LANG_UNKNOWN;
}

static const TSLanguage *cbm_get_language(CbmSupportedLanguage lang) {
    switch (lang) {
        case CBM_LANG_C: return tree_sitter_c();
        case CBM_LANG_TYPESCRIPT: return tree_sitter_typescript();
        case CBM_LANG_PYTHON: return tree_sitter_python();
        default: return NULL;
    }
}

static bool is_comment_or_string(const char *type) {
    if (!type) return false;
    return strcmp(type, "comment") == 0 ||
           strcmp(type, "line_comment") == 0 ||
           strcmp(type, "block_comment") == 0 ||
           strcmp(type, "string") == 0 ||
           strcmp(type, "string_literal") == 0 ||
           strcmp(type, "raw_string_literal") == 0 ||
           strcmp(type, "interpreted_string_literal") == 0;
}

static bool is_decl_node_type(const char *type) {
    if (!type) return false;
    return strcmp(type, "function_definition") == 0 ||
           strcmp(type, "function_declaration") == 0 ||
           strcmp(type, "declaration") == 0 ||
           strcmp(type, "method_definition") == 0 ||
           strcmp(type, "class_declaration") == 0 ||
           strcmp(type, "class_definition") == 0 ||
           strcmp(type, "interface_declaration") == 0 ||
           strcmp(type, "type_alias_declaration") == 0;
}

static TSNode get_c_declarator_identifier(TSNode decl) {
    while (!ts_node_is_null(decl)) {
        const char *t = ts_node_type(decl);
        if (strcmp(t, "identifier") == 0) {
            return decl;
        }
        TSNode inner = ts_node_child_by_field_name(decl, "declarator", 10);
        if (!ts_node_is_null(inner)) {
            decl = inner;
        } else {
            uint32_t count = ts_node_named_child_count(decl);
            bool found = false;
            for (uint32_t i = 0; i < count; i++) {
                TSNode c = ts_node_named_child(decl, i);
                const char *ct = ts_node_type(c);
                if (strcmp(ct, "identifier") == 0) {
                    return c;
                }
                if (strstr(ct, "declarator") != NULL) {
                    decl = c;
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }
    }
    return (TSNode){0};
}

static bool node_declares_symbol(TSNode node, const char *symbol, const char *src) {
    if (!symbol || !symbol[0] || !src) return false;
    size_t sym_len = strlen(symbol);

    /* 1. Direct "name" field (TypeScript / Python) */
    TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
    if (!ts_node_is_null(name_node)) {
        uint32_t s = ts_node_start_byte(name_node);
        uint32_t e = ts_node_end_byte(name_node);
        if ((size_t)(e - s) == sym_len && memcmp(src + s, symbol, sym_len) == 0) {
            return true;
        }
    }

    /* 2. "declarator" field (C) */
    TSNode decl_node = ts_node_child_by_field_name(node, "declarator", 10);
    if (!ts_node_is_null(decl_node)) {
        TSNode id_node = get_c_declarator_identifier(decl_node);
        if (!ts_node_is_null(id_node)) {
            uint32_t s = ts_node_start_byte(id_node);
            uint32_t e = ts_node_end_byte(id_node);
            if ((size_t)(e - s) == sym_len && memcmp(src + s, symbol, sym_len) == 0) {
                return true;
            }
        }
    }

    /* 3. Fallback: check named children before body */
    uint32_t count = ts_node_named_child_count(node);
    for (uint32_t i = 0; i < count; i++) {
        TSNode child = ts_node_named_child(node, i);
        const char *ct = ts_node_type(child);
        if (strcmp(ct, "compound_statement") == 0 ||
            strcmp(ct, "block") == 0 ||
            strcmp(ct, "statement_block") == 0) {
            break;
        }
        if (strcmp(ct, "identifier") == 0 || strcmp(ct, "type_identifier") == 0) {
            uint32_t s = ts_node_start_byte(child);
            uint32_t e = ts_node_end_byte(child);
            if ((size_t)(e - s) == sym_len && memcmp(src + s, symbol, sym_len) == 0) {
                return true;
            }
        }
    }

    return false;
}

static bool node_has_body(TSNode node) {
    if (ts_node_is_null(node)) return false;
    const char *type = ts_node_type(node);
    if (strcmp(type, "function_definition") == 0) return true;
    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; i++) {
        TSNode child = ts_node_child(node, i);
        const char *ct = ts_node_type(child);
        if (strcmp(ct, "compound_statement") == 0 ||
            strcmp(ct, "block") == 0 ||
            strcmp(ct, "statement_block") == 0 ||
            strcmp(ct, "class_body") == 0) {
            return true;
        }
    }
    return false;
}

typedef struct {
    DeclarationNodeMatch matches[16];
    size_t count;
} CandidateMatches;

static void cbm_ts_collect_candidates(TSNode node, const char *symbol, const char *src, CandidateMatches *cands) {
    if (ts_node_is_null(node)) return;

    const char *type = ts_node_type(node);
    if (is_comment_or_string(type)) {
        return;
    }

    if (is_decl_node_type(type) && node_declares_symbol(node, symbol, src)) {
        if (cands->count < sizeof(cands->matches) / sizeof(cands->matches[0])) {
            cands->matches[cands->count].node = node;
            cands->matches[cands->count].start_byte = ts_node_start_byte(node);
            cands->matches[cands->count].end_byte = ts_node_end_byte(node);
            cands->matches[cands->count].node_type = type;
            cands->count++;
        }
        return;
    }

    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; i++) {
        TSNode child = ts_node_child(node, i);
        cbm_ts_collect_candidates(child, symbol, src, cands);
    }
}

bool cbm_ts_find_declaration_node(TSNode root, const char *symbol, const char *src, DeclarationNodeMatch *out) {
    if (out) {
        memset(out, 0, sizeof(*out));
    }
    if (ts_node_is_null(root) || !symbol || !symbol[0] || !src || !out) {
        return false;
    }

    CandidateMatches cands = {0};
    cbm_ts_collect_candidates(root, symbol, src, &cands);
    if (cands.count == 0) return false;

    /* Prefer full definitions with a body (e.g. function_definition with compound_statement) */
    for (size_t i = 0; i < cands.count; i++) {
        if (node_has_body(cands.matches[i].node)) {
            *out = cands.matches[i];
            return true;
        }
    }

    /* Fallback: return the first declaration (e.g. prototype in header) */
    *out = cands.matches[0];
    return true;
}

static inline void fnv1a_update(uint64_t *h, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        *h ^= p[i];
        *h *= 0x100000001b3ULL;
    }
}

static void hash_walk(TSNode node, const char *src, StructuralHashContext *ctx) {
    if (ts_node_is_null(node)) return;

    const char *type = ts_node_type(node);
    if (ctx->skip_comments && (strcmp(type, "comment") == 0 ||
                               strcmp(type, "line_comment") == 0 ||
                               strcmp(type, "block_comment") == 0)) {
        return;
    }

    ctx->visited_nodes++;
    size_t type_len = strlen(type);
    fnv1a_update(&ctx->running_fnv, type, type_len);
    uint8_t sep = 0x1f;
    fnv1a_update(&ctx->running_fnv, &sep, 1);

    uint32_t child_count = ts_node_child_count(node);
    if (child_count == 0) {
        uint32_t s = ts_node_start_byte(node);
        uint32_t e = ts_node_end_byte(node);
        if (e > s && src) {
            fnv1a_update(&ctx->running_fnv, src + s, e - s);
            fnv1a_update(&ctx->running_fnv, &sep, 1);
        }
    } else {
        for (uint32_t i = 0; i < child_count; i++) {
            TSNode child = ts_node_child(node, i);
            hash_walk(child, src, ctx);
        }
    }
}

uint64_t cbm_ts_compute_declaration_hash(TSNode decl_node, const char *src) {
    if (ts_node_is_null(decl_node) || !src) return 0;
    StructuralHashContext ctx;
    ctx.running_fnv = 0xcbf29ce484222325ULL;
    ctx.visited_nodes = 0;
    ctx.skip_comments = true;
    ctx.skip_whitespace = true;

    hash_walk(decl_node, src, &ctx);
    return ctx.running_fnv;
}

bool cbm_fast_offset_match(const char *root, const TwoTierAnchor *anchor) {
    if (!anchor || anchor->byte_len == 0 || anchor->ast_signature_hash == 0) return false;
    if (anchor->byte_len > sizeof(anchor->expected_text)) return false;

    char full_path[1024];
    if (!make_file_path(root, anchor->file_path, full_path, sizeof(full_path))) {
        return false;
    }

    FILE *f = fopen(full_path, "rb");
    if (!f) return false;

    if (fseek(f, (long)anchor->byte_start, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    char *buf = (char *)malloc(anchor->byte_len + 1);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t read_bytes = fread(buf, 1, anchor->byte_len, f);
    fclose(f);

    if (read_bytes != anchor->byte_len) {
        free(buf);
        return false;
    }
    buf[read_bytes] = '\0';

    bool match = (memcmp(buf, anchor->expected_text, anchor->byte_len) == 0);
    free(buf);
    return match;
}

int cbm_verify_two_tier_anchor(const char *root, const TwoTierAnchor *anchor, bool *ok, TwoTierAnchorRelocation *out_relocation) {
    if (ok) *ok = false;
    if (out_relocation) {
        memset(out_relocation, 0, sizeof(*out_relocation));
    }
    if (!anchor || !ok || anchor->ast_signature_hash == 0) return -1;

    char full_path[1024];
    if (!make_file_path(root, anchor->file_path, full_path, sizeof(full_path))) {
        *ok = false;
        return -1;
    }

    FILE *f = fopen(full_path, "rb");
    if (!f) {
        *ok = false;
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 10 * 1024 * 1024) {
        fclose(f);
        *ok = false;
        return 0;
    }

    char *content = (char *)malloc(sz + 1);
    if (!content) {
        fclose(f);
        return -1;
    }

    size_t read_bytes = fread(content, 1, sz, f);
    fclose(f);
    content[read_bytes] = '\0';
    size_t remaining = read_bytes;
    (void)remaining;

    CbmSupportedLanguage lang = cbm_resolve_language_from_path(anchor->file_path);
    const TSLanguage *ts_lang = cbm_get_language(lang);
    if (!ts_lang) {
        /* Unsupported language: cannot parse AST */
        free(content);
        *ok = false;
        return 0;
    }

    TSParser *parser = ts_parser_new();
    if (!parser) {
        free(content);
        return -1;
    }
    ts_parser_set_language(parser, ts_lang);

    TSTree *tree = ts_parser_parse_string(parser, NULL, content, read_bytes);
    if (!tree) {
        ts_parser_delete(parser);
        free(content);
        *ok = false;
        return 0;
    }

    TSNode root_node = ts_tree_root_node(tree);
    DeclarationNodeMatch decl_match = {0};
    bool found = cbm_ts_find_declaration_node(root_node, anchor->symbol_name, content, &decl_match);
    if (!found) {
        /* Symbol missing or only appears in comment/string literal */
        ts_tree_delete(tree);
        ts_parser_delete(parser);
        free(content);
        *ok = false;
        return 0;
    }

    uint32_t start = decl_match.start_byte;
    uint32_t len = decl_match.end_byte - start;
    uint64_t hash = cbm_ts_compute_declaration_hash(decl_match.node, content);

    if (hash != anchor->ast_signature_hash) {
        /* Check other candidate matches if multiple exist */
        CandidateMatches cands = {0};
        cbm_ts_collect_candidates(root_node, anchor->symbol_name, content, &cands);
        bool any_matched = false;
        for (size_t i = 0; i < cands.count; i++) {
            uint64_t cand_hash = cbm_ts_compute_declaration_hash(cands.matches[i].node, content);
            if (cand_hash == anchor->ast_signature_hash) {
                decl_match = cands.matches[i];
                start = decl_match.start_byte;
                len = decl_match.end_byte - start;
                hash = cand_hash;
                any_matched = true;
                break;
            }
        }
        if (!any_matched) {
            ts_tree_delete(tree);
            ts_parser_delete(parser);
            free(content);
            *ok = false;
            return 0;
        }
    }

    *ok = true;
    if (start == anchor->byte_start) {
        /* TRUE Fast-Path match in the real file */
        if (out_relocation) {
            out_relocation->was_relocated = false;
            out_relocation->structural_hash_matched = true;
            out_relocation->new_byte_start = start;
            out_relocation->new_byte_len = len;
        }
    } else {
        /* Tier 2 Relocation */
        if (out_relocation) {
            out_relocation->was_relocated = true;
            out_relocation->structural_hash_matched = true;
            out_relocation->new_byte_start = start;
            out_relocation->new_byte_len = len;
        }
    }

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    free(content);
    return 0;
}

int cbm_ast_signature_match(const char *root, const TwoTierAnchor *anchor, bool *ok) {
    if (ok) *ok = false;
    if (!anchor || !ok) return -1;
    return cbm_verify_two_tier_anchor(root, anchor, ok, NULL);
}

