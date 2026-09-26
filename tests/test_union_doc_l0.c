/*
 * test_union_doc_l0.c — Scope C04 acceptance criteria (Structural Extraction L0).
 */
#include "test_framework.h"
#include "../src/union/union_doc_l0.h"
#include "../src/union/union_refusal.h"

#include <string.h>

typedef struct {
    char path[256];
    char content[4096];
} MockDocFile;

static bool mock_doc_reader(const char *path, size_t start, size_t len,
                            char *out_buf, size_t buf_sz, void *ctx) {
    MockDocFile *file = (MockDocFile *)ctx;
    if (!file || strcmp(file->path, path) != 0) return false;
    size_t file_len = strlen(file->content);
    if (start >= file_len || start + len > file_len) return false;
    if (len >= buf_sz) return false;

    memcpy(out_buf, file->content + start, len);
    out_buf[len] = '\0';
    return true;
}

/* AC1: Given markdown file with N headings -> N section nodes with slug URIs,
 * each with FILE_BYTES anchor verified against file. */
TEST(test_doc_l0_headings_and_anchors) {
    const char *md =
        "---\n"
        "title: Security Overview\n"
        "status: draft\n"
        "---\n\n"
        "# Security Overview\n"
        "Intro text.\n\n"
        "## §3.7 Emendation II\n"
        "The documentary plane details.\n\n"
        "### Key Principles\n"
        "Bullet points here.\n";

    MockDocFile mock;
    strncpy(mock.path, "docs/security.md", sizeof(mock.path) - 1);
    strncpy(mock.content, md, sizeof(mock.content) - 1);

    CbmDocL0ParseResult res;
    char reason[128] = {0};
    CbmRefusalCode code = cbm_doc_parse_l0("docs/security.md", md, &res, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    /* 3 Headings -> 3 sections */
    ASSERT_EQ(res.section_count, 3);

    /* First section */
    ASSERT_STR_EQ(res.sections[0].heading_title, "Security Overview");
    ASSERT_STR_EQ(res.sections[0].heading_slug, "security-overview");
    ASSERT_STR_EQ(res.sections[0].uri, "cbm://docs/security.md#security-overview");
    ASSERT_EQ(res.sections[0].level, 1);
    ASSERT_EQ(res.sections[0].anchor.kind, CBM_ANCHOR_FILE_BYTES);

    /* Second section: ignores numeric/section prefix §3.7 */
    ASSERT_STR_EQ(res.sections[1].heading_title, "§3.7 Emendation II");
    ASSERT_STR_EQ(res.sections[1].heading_slug, "emendation-ii");
    ASSERT_STR_EQ(res.sections[1].uri, "cbm://docs/security.md#emendation-ii");
    ASSERT_EQ(res.sections[1].level, 2);

    /* Verify FILE_BYTES anchors against mock */
    CbmAnchorEnv env = {0};
    env.file_reader = mock_doc_reader;
    env.file_reader_ctx = &mock;

    for (size_t i = 0; i < res.section_count; i++) {
        cbm_doc_section_update_status(&res.sections[i], &env);
        ASSERT_TRUE(res.sections[i].is_verified);
        ASSERT_FALSE(res.sections[i].is_gone);
    }

    /* Check frontmatter properties */
    ASSERT_EQ(res.property_count, 2);
    ASSERT_STR_EQ(res.properties[0].key, "title");
    ASSERT_STR_EQ(res.properties[0].value, "Security Overview");
    ASSERT_STR_EQ(res.properties[1].key, "status");
    ASSERT_STR_EQ(res.properties[1].value, "draft");

    PASS();
}

/* AC2 & AC3: Heading renamed or section deleted -> anchor verification reflects gone/drift */
TEST(test_doc_l0_section_deleted_or_drifted) {
    const char *md =
        "# First Heading\n"
        "Content 1.\n\n"
        "# Second Heading\n"
        "Content 2.\n";

    MockDocFile mock;
    strncpy(mock.path, "docs/test.md", sizeof(mock.path) - 1);
    strncpy(mock.content, md, sizeof(mock.content) - 1);

    CbmDocL0ParseResult res;
    (void)cbm_doc_parse_l0("docs/test.md", md, &res, NULL, 0);
    ASSERT_EQ(res.section_count, 2);

    CbmAnchorEnv env = {0};
    env.file_reader = mock_doc_reader;
    env.file_reader_ctx = &mock;

    cbm_doc_section_update_status(&res.sections[1], &env);
    ASSERT_TRUE(res.sections[1].is_verified);

    /* Now file is modified and second heading is deleted */
    strncpy(mock.content, "# First Heading\nContent 1 only.\n", sizeof(mock.content) - 1);

    /* Re-check status of section 2 */
    cbm_doc_section_update_status(&res.sections[1], &env);
    ASSERT_FALSE(res.sections[1].is_verified);
    ASSERT_TRUE(res.sections[1].is_gone);

    PASS();
}

/* AC4: Document with internal links -> recorded as unresolved reference candidates */
TEST(test_doc_l0_extracts_internal_links) {
    const char *md =
        "# Architecture\n\n"
        "See [ADR 001](docs/adr/001-protocol.md) and [CBM Schema](cbm://repo/src/core#CbmUri).\n"
        "Also see [Relative Link](#key-principles).\n";

    CbmDocL0ParseResult res;
    CbmRefusalCode code = cbm_doc_parse_l0("docs/arch.md", md, &res, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    ASSERT_EQ(res.link_count, 3);
    ASSERT_STR_EQ(res.links[0].target_ref, "docs/adr/001-protocol.md");
    ASSERT_FALSE(res.links[0].is_cbm_uri);

    ASSERT_STR_EQ(res.links[1].target_ref, "cbm://repo/src/core#CbmUri");
    ASSERT_TRUE(res.links[1].is_cbm_uri);

    ASSERT_STR_EQ(res.links[2].target_ref, "#key-principles");

    PASS();
}

/* AC5: Section status derives from its anchors (verified or gone) */
TEST(test_doc_l0_section_status_derivation) {
    CbmDocSectionNode node = {0};
    node.anchor.kind = CBM_ANCHOR_FILE_BYTES;
    strncpy(node.anchor.file_bytes.file_path, "missing_file.md",
            sizeof(node.anchor.file_bytes.file_path) - 1);
    node.anchor.file_bytes.byte_len = 10;
    strncpy(node.anchor.file_bytes.expected_text, "test",
            sizeof(node.anchor.file_bytes.expected_text) - 1);

    /* Check with empty env (file does not exist) */
    CbmAnchorEnv env = {0};
    cbm_doc_section_update_status(&node, &env);

    ASSERT_FALSE(node.is_verified);
    ASSERT_TRUE(node.is_gone);

    PASS();
}

/* AC6: Malformed markdown (unclosed code fence or truncated frontmatter) -> refusal */
TEST(test_doc_l0_malformed_markdown_refusal) {
    CbmDocL0ParseResult res;
    char reason[128] = {0};

    /* Unclosed frontmatter */
    const char *bad_frontmatter =
        "---\n"
        "title: Broken Document\n"
        "status: draft\n"
        "# Heading without frontmatter terminator\n";

    CbmRefusalCode code = cbm_doc_parse_l0("docs/bad.md", bad_frontmatter, &res, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "frontmatter") != NULL);

    /* Unclosed code block */
    const char *bad_code_block =
        "# Heading\n"
        "```c\n"
        "int main() { return 0; }\n"
        "no closing backticks\n";

    code = cbm_doc_parse_l0("docs/bad2.md", bad_code_block, &res, reason, sizeof(reason));
    ASSERT_EQ(code, CBM_REFUSAL_CLAIM_INVALID);
    ASSERT_TRUE(strstr(reason, "code block") != NULL);

    PASS();
}

SUITE(union_doc_l0) {
    RUN_TEST(test_doc_l0_headings_and_anchors);
    RUN_TEST(test_doc_l0_section_deleted_or_drifted);
    RUN_TEST(test_doc_l0_extracts_internal_links);
    RUN_TEST(test_doc_l0_section_status_derivation);
    RUN_TEST(test_doc_l0_malformed_markdown_refusal);
}
