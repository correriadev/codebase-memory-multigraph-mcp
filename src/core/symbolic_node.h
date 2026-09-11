#ifndef CBM_SYMBOLIC_NODE_H
#define CBM_SYMBOLIC_NODE_H

#include "cbm_uri.h"
#include "visited_set.h"
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

typedef enum {
    EPISTEMIC_PROPOSED = 0,
    EPISTEMIC_ACCEPTED = 1,
    EPISTEMIC_CONTESTED = 2,
    EPISTEMIC_SHADOWED = 3
} EpistemicStatus;

typedef enum {
    EDGE_VIRTUAL_CALLS = 0,
    EDGE_REPLACES = 1,
    EDGE_EXTENDS = 2,
    EDGE_REFERENCES = 3
} EdgeType;

typedef struct {
    CbmUri uri;
    char label[64];
    EpistemicStatus status;
    bool is_dangling;
} SymbolicNode;

typedef struct {
    CbmUri source;
    CbmUri target;
    EdgeType type;
    char origin_horizon[64];
} VirtualEdge;

/* Insert or update a proposed symbolic node in a horizon SQLite DB */
int cbm_symbolic_node_insert(sqlite3 *db, const SymbolicNode *node, const char *code_snippet);

/* Find a symbolic node by its CBM-URI */
int cbm_symbolic_node_find(sqlite3 *db, const char *cbm_uri_str, SymbolicNode *out_node, char *out_snippet, size_t snippet_sz);

/* Insert a virtual edge between nodes */
int cbm_virtual_edge_insert(sqlite3 *db, const VirtualEdge *edge);

/* Query outbound virtual edges from source URI */
int cbm_virtual_edge_find_outbound(sqlite3 *db, const char *source_uri, VirtualEdge *out_edges, size_t max_edges, size_t *out_count);

/* Query inbound virtual edges to target URI */
int cbm_virtual_edge_find_inbound(sqlite3 *db, const char *target_uri, VirtualEdge *out_edges, size_t max_edges, size_t *out_count);

/* Acyclic BFS traversal of dangling / symbolic nodes using VisitedSet */
int cbm_traverse_symbolic_bfs(sqlite3 *db, const char *start_uri, bool reverse, VisitedSet *visited, uint32_t max_depth, char out_visited_uris[][CBM_URI_MAX_LEN], size_t max_out, size_t *out_count);

#endif /* CBM_SYMBOLIC_NODE_H */
