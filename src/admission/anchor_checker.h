#ifndef CBM_ANCHOR_CHECKER_H
#define CBM_ANCHOR_CHECKER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

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

/* Tier 1 Fast Path: match exact byte offset in file */
bool cbm_fast_offset_match(const char *root, const TwoTierAnchor *anchor);

/* Tier 2 Fallback: locate symbol and match AST signature hash */
int cbm_ast_signature_match(const char *root, const TwoTierAnchor *anchor, bool *ok);

/* Two-Tier anchor verification */
int cbm_verify_two_tier_anchor(const char *root, const TwoTierAnchor *anchor, bool *ok);

#endif /* CBM_ANCHOR_CHECKER_H */
