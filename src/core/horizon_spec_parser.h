#ifndef CBM_HORIZON_SPEC_PARSER_H
#define CBM_HORIZON_SPEC_PARSER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(__has_include)
  #if __has_include(<sqlite3.h>)
    #include <sqlite3.h>
  #elif __has_include("sqlite3.h")
    #include "sqlite3.h"
  #endif
#else
  #include <sqlite3.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t nodes_compiled;
    size_t edges_compiled;
    size_t sections_indexed;
} TacticalSpecCompileReport;

/**
 * Compiles declarative tactical specification blocks (```tactical-spec ...)
 * and indexes CommonMark sections from markdown_text into an ephemeral horizon database.
 *
 * Wrapped in an atomic BEGIN IMMEDIATE / COMMIT transaction (with ROLLBACK on error).
 *
 * 1. Extracts tactical-spec code blocks (JSON or YAML).
 *    For each node:
 *      - Determines cbm_uri (explicit or cbm://<repo_name>/<target_path>#<symbol>)
 *      - label = node type (AggregateRoot, DomainService, etc.) or "Symbol"
 *      - epistemic_status = 'PROPOSED'
 *      - is_dangling = 1
 *      - code_snippet = description and/or method signatures
 *      - Persists to symbolic_nodes via INSERT OR REPLACE
 *    For each edge:
 *      - Resolves source_uri and target_uri against nodes in this block or full cbm:// URI
 *      - edge_type = edge type (CALLS, USES, EXTENDS, etc.)
 *      - origin_horizon = origin_horizon ID
 *      - Persists to virtual_edges via INSERT OR REPLACE
 *
 * 2. Indexes Markdown headings (# Title, ## Subtitle, etc.):
 *    - Deletes prior spec_fts entries for file_path (for idempotency)
 *    - Generates slugified heading anchors (e.g. event-storming)
 *    - Stores section text in symbolic_nodes (label='Section', is_dangling=1, PROPOSED)
 *    - Inserts into spec_fts (file_path, heading_slug, title, content)
 *
 * @param hdb             Open SQLite database handle for the horizon
 * @param origin_horizon  Identifier of the horizon
 * @param repo_name       Repository name for canonical CBM URIs
 * @param file_path       Relative path of the markdown spec file
 * @param markdown_text   Content of the markdown file
 * @param report          Output statistics report (may be NULL)
 * @param err_buf         Error buffer for diagnostics (may be NULL)
 * @param err_sz          Size of err_buf
 * @return 0 on success, non-zero on failure
 */
int cbm_compile_spec_to_horizon(sqlite3 *hdb,
                                const char *origin_horizon,
                                const char *repo_name,
                                const char *file_path,
                                const char *markdown_text,
                                TacticalSpecCompileReport *report,
                                char *err_buf,
                                size_t err_sz);

#ifdef __cplusplus
}
#endif

#endif /* CBM_HORIZON_SPEC_PARSER_H */
