#ifndef CBM_ANCHOR_CHECKER_H
#define CBM_ANCHOR_CHECKER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "tree_sitter/api.h"

#define CBM_ANCHOR_FILE_PATH_CAPACITY 512U
#define CBM_ANCHOR_SYMBOL_NAME_CAPACITY 256U
#define CBM_ANCHOR_EXPECTED_TEXT_CAPACITY 1024U

typedef struct {
    char file_path[CBM_ANCHOR_FILE_PATH_CAPACITY];
    char symbol_name[CBM_ANCHOR_SYMBOL_NAME_CAPACITY];
    uint32_t byte_start;
    uint32_t byte_len;
    uint64_t ast_signature_hash;
    char expected_text[CBM_ANCHOR_EXPECTED_TEXT_CAPACITY];
} TwoTierAnchor;

typedef struct {
    uint32_t new_byte_start;
    uint32_t new_byte_len;
    bool was_relocated;
    bool structural_hash_matched;
} TwoTierAnchorRelocation;

#ifndef CBM_H
typedef enum {
    CBM_LANG_UNKNOWN = 0,
    CBM_LANG_C = 1,
    CBM_LANG_TYPESCRIPT = 2,
    CBM_LANG_PYTHON = 3
} CbmSupportedLanguage;
#else
typedef int CbmSupportedLanguage;
#ifndef CBM_LANG_UNKNOWN
#define CBM_LANG_UNKNOWN 0
#endif
#endif

typedef struct {
    TSNode node;
    uint32_t start_byte;
    uint32_t end_byte;
    const char *node_type;
} DeclarationNodeMatch;

typedef struct {
    uint64_t running_fnv;
    size_t visited_nodes;
    bool skip_comments;
    bool skip_whitespace;
} StructuralHashContext;

/* Tier 1 Fast Path: match exact byte offset in file */
bool cbm_fast_offset_match(const char *root, const TwoTierAnchor *anchor);

/* Tier 2 Fallback: locate symbol and match AST signature hash */
int cbm_ast_signature_match(const char *root, const TwoTierAnchor *anchor, bool *ok);

/* Two-Tier anchor verification with optional coordinate relocation */
int cbm_verify_two_tier_anchor(const char *root, const TwoTierAnchor *anchor, bool *ok, TwoTierAnchorRelocation *out_relocation);

/* Language resolution from file path */
CbmSupportedLanguage cbm_resolve_language_from_path(const char *path);

/* Tree-sitter declaration node lookup */
bool cbm_ts_find_declaration_node(TSNode root, const char *symbol, const char *src, DeclarationNodeMatch *out);

/* Structural declaration AST signature hashing */
uint64_t cbm_ts_compute_declaration_hash(TSNode decl_node, const char *src);

#endif /* CBM_ANCHOR_CHECKER_H */
