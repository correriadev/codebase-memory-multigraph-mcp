#include "anchor_checker.h"
#include "../core/cbm_uri.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void make_file_path(const char *root, const char *rel, char *out, size_t out_sz) {
    if (root && root[0]) {
        snprintf(out, out_sz, "%s/%s", root, rel);
    } else {
        snprintf(out, out_sz, "%s", rel);
    }
}

bool cbm_fast_offset_match(const char *root, const TwoTierAnchor *anchor) {
    if (!anchor || anchor->byte_len == 0) return false;
    if (anchor->byte_len > sizeof(anchor->expected_text)) return false;

    char full_path[1024];
    make_file_path(root, anchor->file_path, full_path, sizeof(full_path));

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

/* Helper to compute FNV-1a 64-bit hash normalizing whitespace and CRLF */
static uint64_t cbm_normalized_fnv1a_64(const char *s, size_t len) {
    if (!s || len == 0) return 0;
    uint64_t h = 0xcbf29ce484222325ULL;
    bool in_ws = false;
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c == '\r') continue;
        if (c == ' ' || c == '\t' || c == '\n') {
            if (!in_ws) {
                h ^= (uint8_t)' ';
                h *= 0x100000001b3ULL;
                in_ws = true;
            }
        } else {
            in_ws = false;
            h ^= (uint8_t)c;
            h *= 0x100000001b3ULL;
        }
    }
    return h;
}

int cbm_ast_signature_match(const char *root, const TwoTierAnchor *anchor, bool *ok) {
    if (!anchor || !ok) return -1;
    *ok = false;

    char full_path[1024];
    make_file_path(root, anchor->file_path, full_path, sizeof(full_path));

    FILE *f = fopen(full_path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 10 * 1024 * 1024) {
        fclose(f);
        return -1;
    }

    char *content = (char *)malloc(sz + 1);
    if (!content) {
        fclose(f);
        return -1;
    }

    size_t read_bytes = fread(content, 1, sz, f);
    fclose(f);
    content[read_bytes] = '\0';

    /* Search for the symbol in the file content (e.g. function or declaration header) */
    char *found = NULL;
    if (anchor->expected_text[0]) {
        found = strstr(content, anchor->expected_text);
    }
    if (!found && anchor->symbol_name[0]) {
        found = strstr(content, anchor->symbol_name);
    }

    if (found) {
        /* Compute AST signature hash on the located slice or text */
        size_t match_len = anchor->byte_len > 0 ? anchor->byte_len : strlen(anchor->expected_text);
        if (match_len > sizeof(anchor->expected_text) && anchor->expected_text[0]) {
            match_len = strlen(anchor->expected_text);
        }
        size_t rem = (size_t)(content + read_bytes - found);
        if (rem < match_len) {
            *ok = false;
        } else {
            uint64_t computed_hash = cbm_fnv1a_64(found, match_len);
            if (computed_hash == anchor->ast_signature_hash) {
                *ok = true;
            } else if (anchor->expected_text[0]) {
                /* Whitespace/CRLF resilient AST normalization */
                uint64_t norm_found = cbm_normalized_fnv1a_64(found, match_len);
                uint64_t norm_exp = cbm_normalized_fnv1a_64(anchor->expected_text, strlen(anchor->expected_text));
                *ok = (norm_found != 0 && norm_found == norm_exp);
            } else {
                *ok = false;
            }
        }
    } else {
        *ok = false;
    }

    free(content);
    return 0;
}

int cbm_verify_two_tier_anchor(const char *root, const TwoTierAnchor *anchor, bool *ok) {
    if (!anchor || !ok) return -1;

    /* Tier 1 Fast Path */
    if (cbm_fast_offset_match(root, anchor)) {
        *ok = true;
        return 0;
    }

    /* Tier 2 AST Signature Fallback */
    return cbm_ast_signature_match(root, anchor, ok);
}
