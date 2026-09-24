#include "horizon_spec_parser.h"
#include "cbm_uri.h"
#include "symbolic_node.h"

#if defined(__has_include)
  #if __has_include(<yyjson/yyjson.h>)
    #include <yyjson/yyjson.h>
  #elif __has_include("yyjson/yyjson.h")
    #include "yyjson/yyjson.h"
  #elif __has_include("yyjson.h")
    #include "yyjson.h"
  #endif
#else
  #include <yyjson/yyjson.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#if defined(_WIN32)
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#else
#include <strings.h>
#endif

/* ── Dynamic String Buffer ────────────────────────────────────────── */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static void strbuf_init(StrBuf *sb) {
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void strbuf_free(StrBuf *sb) {
    if (sb->data) {
        free(sb->data);
        sb->data = NULL;
    }
    sb->len = 0;
    sb->cap = 0;
}

static void strbuf_append(StrBuf *sb, const char *str, size_t n) {
    if (!str || n == 0) return;
    if (sb->len + n + 1 > sb->cap) {
        size_t new_cap = sb->cap ? sb->cap * 2 : 256;
        while (new_cap < sb->len + n + 1) new_cap *= 2;
        char *new_data = (char *)realloc(sb->data, new_cap);
        if (!new_data) return;
        sb->data = new_data;
        sb->cap = new_cap;
    }
    memcpy(sb->data + sb->len, str, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void strbuf_append_str(StrBuf *sb, const char *str) {
    if (str) strbuf_append(sb, str, strlen(str));
}

/* ── Path and String Normalization ────────────────────────────────── */

static void normalize_path(const char *in, char *out, size_t out_sz) {
    if (!in || !out || out_sz == 0) return;
    while (*in == ' ' || *in == '\t') in++;
    if (in[0] == '.' && (in[1] == '/' || in[1] == '\\')) {
        in += 2;
    }
    while (*in == '/' || *in == '\\') in++;

    size_t j = 0;
    while (*in && j + 1 < out_sz) {
        if (*in == '\\') {
            out[j++] = '/';
        } else {
            out[j++] = *in;
        }
        in++;
    }
    while (j > 0 && (out[j - 1] == ' ' || out[j - 1] == '\t' ||
                     out[j - 1] == '\r' || out[j - 1] == '\n')) {
        j--;
    }
    out[j] = '\0';
}

static char *clean_str_dup(const char *src) {
    if (!src) return NULL;
    while (*src == ' ' || *src == '\t' || *src == '\r' || *src == '\n') src++;
    size_t len = strlen(src);
    while (len > 0 && (src[len - 1] == ' ' || src[len - 1] == '\t' ||
                       src[len - 1] == '\r' || src[len - 1] == '\n')) {
        len--;
    }
    if (len >= 2 && ((src[0] == '"' && src[len - 1] == '"') ||
                     (src[0] == '\'' && src[len - 1] == '\''))) {
        src++;
        len -= 2;
    }
    char *res = (char *)malloc(len + 1);
    if (res) {
        memcpy(res, src, len);
        res[len] = '\0';
    }
    return res;
}

/* ── Heading Slugification with Collision Avoidance ───────────────── */

typedef struct {
    char slugs[512][128];
    size_t count;
} SlugRegistry;

static void slug_registry_init(SlugRegistry *reg) {
    reg->count = 0;
}

static void slugify_heading(const char *title, char *out_slug, size_t out_sz, SlugRegistry *reg) {
    if (!title || !out_slug || out_sz == 0) return;
    char base_slug[96];
    size_t out_idx = 0;
    bool last_was_dash = true;

    for (size_t i = 0; title[i] != '\0' && out_idx + 1 < sizeof(base_slug); i++) {
        unsigned char c = (unsigned char)title[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            base_slug[out_idx++] = (char)c;
            last_was_dash = false;
        } else if (c >= 'A' && c <= 'Z') {
            base_slug[out_idx++] = (char)(c + ('a' - 'A'));
            last_was_dash = false;
        } else if (c == ' ' || c == '-' || c == '_' || c == '.' || c == ':' || c == '/' || c == '\\') {
            if (!last_was_dash && out_idx + 1 < sizeof(base_slug)) {
                base_slug[out_idx++] = '-';
                last_was_dash = true;
            }
        }
    }
    while (out_idx > 0 && base_slug[out_idx - 1] == '-') {
        out_idx--;
    }
    base_slug[out_idx] = '\0';
    if (out_idx == 0) {
        snprintf(base_slug, sizeof(base_slug), "section");
    }

    char candidate[256];
    snprintf(candidate, sizeof(candidate), "%s", base_slug);
    int suffix = 1;
    bool collision = true;
    while (collision) {
        collision = false;
        if (reg) {
            for (size_t k = 0; k < reg->count; k++) {
                if (strcmp(reg->slugs[k], candidate) == 0) {
                    collision = true;
                    snprintf(candidate, sizeof(candidate), "%s-%d", base_slug, suffix++);
                    break;
                }
            }
        }
    }
    if (reg && reg->count < 512) {
        size_t slen = strlen(candidate);
        if (slen >= sizeof(reg->slugs[0])) slen = sizeof(reg->slugs[0]) - 1;
        memcpy(reg->slugs[reg->count], candidate, slen);
        reg->slugs[reg->count][slen] = '\0';
        reg->count++;
    }
    size_t olen = strlen(candidate);
    if (olen >= out_sz) olen = out_sz - 1;
    memcpy(out_slug, candidate, olen);
    out_slug[olen] = '\0';
}

/* ── Intermediate Spec Entities ───────────────────────────────────── */

typedef struct {
    char *symbol;
    char *type;
    char *target_path;
    char *cbm_uri;
    char *description;
    StrBuf methods;
} SpecNode;

typedef struct {
    char *source;
    char *target;
    char *type;
} SpecEdge;

typedef struct {
    SpecNode *nodes;
    size_t node_count;
    size_t node_cap;
    SpecEdge *edges;
    size_t edge_count;
    size_t edge_cap;
} SpecCollection;

static void spec_collection_init(SpecCollection *sc) {
    memset(sc, 0, sizeof(*sc));
}

static void spec_node_free(SpecNode *node) {
    if (!node) return;
    free(node->symbol);
    free(node->type);
    free(node->target_path);
    free(node->cbm_uri);
    free(node->description);
    strbuf_free(&node->methods);
}

static void spec_edge_free(SpecEdge *edge) {
    if (!edge) return;
    free(edge->source);
    free(edge->target);
    free(edge->type);
}

static void spec_collection_free(SpecCollection *sc) {
    if (!sc) return;
    for (size_t i = 0; i < sc->node_count; i++) {
        spec_node_free(&sc->nodes[i]);
    }
    free(sc->nodes);
    for (size_t i = 0; i < sc->edge_count; i++) {
        spec_edge_free(&sc->edges[i]);
    }
    free(sc->edges);
    memset(sc, 0, sizeof(*sc));
}

static SpecNode *spec_collection_add_node(SpecCollection *sc) {
    if (sc->node_count >= sc->node_cap) {
        size_t new_cap = sc->node_cap ? sc->node_cap * 2 : 16;
        SpecNode *new_arr = (SpecNode *)realloc(sc->nodes, new_cap * sizeof(SpecNode));
        if (!new_arr) return NULL;
        sc->nodes = new_arr;
        sc->node_cap = new_cap;
    }
    SpecNode *node = &sc->nodes[sc->node_count++];
    memset(node, 0, sizeof(*node));
    strbuf_init(&node->methods);
    return node;
}

static SpecEdge *spec_collection_add_edge(SpecCollection *sc) {
    if (sc->edge_count >= sc->edge_cap) {
        size_t new_cap = sc->edge_cap ? sc->edge_cap * 2 : 16;
        SpecEdge *new_arr = (SpecEdge *)realloc(sc->edges, new_cap * sizeof(SpecEdge));
        if (!new_arr) return NULL;
        sc->edges = new_arr;
        sc->edge_cap = new_cap;
    }
    SpecEdge *edge = &sc->edges[sc->edge_count++];
    memset(edge, 0, sizeof(*edge));
    return edge;
}

/* ── JSON Tactical Spec Parsing ───────────────────────────────────── */

static bool parse_json_tactical_spec(const char *body, SpecCollection *sc) {
    if (!body) return false;
    while (*body == ' ' || *body == '\t' || *body == '\r' || *body == '\n') body++;
    if (*body != '{' && *body != '[') return false;

    yyjson_read_flag flags = YYJSON_READ_ALLOW_COMMENTS | YYJSON_READ_ALLOW_TRAILING_COMMAS;
    yyjson_doc *doc = yyjson_read(body, strlen(body), flags);
    if (!doc) return false;

    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return false;
    }

    yyjson_val *nodes_arr = yyjson_obj_get(root, "nodes");
    if (yyjson_is_arr(nodes_arr)) {
        size_t idx, max;
        yyjson_val *nval;
        yyjson_arr_foreach(nodes_arr, idx, max, nval) {
            if (!yyjson_is_obj(nval)) continue;
            SpecNode *node = spec_collection_add_node(sc);
            if (!node) continue;

            const char *sym = yyjson_get_str(yyjson_obj_get(nval, "symbol"));
            if (sym) node->symbol = clean_str_dup(sym);

            const char *typ = yyjson_get_str(yyjson_obj_get(nval, "type"));
            if (typ) node->type = clean_str_dup(typ);

            const char *tgt = yyjson_get_str(yyjson_obj_get(nval, "target_path"));
            if (!tgt) tgt = yyjson_get_str(yyjson_obj_get(nval, "path"));
            if (tgt) node->target_path = clean_str_dup(tgt);

            const char *uri = yyjson_get_str(yyjson_obj_get(nval, "cbm_uri"));
            if (!uri) uri = yyjson_get_str(yyjson_obj_get(nval, "uri"));
            if (uri) node->cbm_uri = clean_str_dup(uri);

            const char *desc = yyjson_get_str(yyjson_obj_get(nval, "description"));
            if (desc) node->description = clean_str_dup(desc);

            yyjson_val *meth_val = yyjson_obj_get(nval, "methods");
            if (yyjson_is_arr(meth_val)) {
                size_t m_idx, m_max;
                yyjson_val *m_item;
                yyjson_arr_foreach(meth_val, m_idx, m_max, m_item) {
                    const char *m_str = yyjson_get_str(m_item);
                    if (m_str) {
                        strbuf_append_str(&node->methods, "- ");
                        strbuf_append_str(&node->methods, m_str);
                        strbuf_append_str(&node->methods, "\n");
                    }
                }
            } else if (yyjson_is_str(meth_val)) {
                strbuf_append_str(&node->methods, yyjson_get_str(meth_val));
                strbuf_append_str(&node->methods, "\n");
            }
        }
    }

    yyjson_val *edges_arr = yyjson_obj_get(root, "edges");
    if (yyjson_is_arr(edges_arr)) {
        size_t idx, max;
        yyjson_val *eval;
        yyjson_arr_foreach(edges_arr, idx, max, eval) {
            if (!yyjson_is_obj(eval)) continue;
            SpecEdge *edge = spec_collection_add_edge(sc);
            if (!edge) continue;

            const char *src = yyjson_get_str(yyjson_obj_get(eval, "source"));
            if (src) edge->source = clean_str_dup(src);

            const char *tgt = yyjson_get_str(yyjson_obj_get(eval, "target"));
            if (tgt) edge->target = clean_str_dup(tgt);

            const char *typ = yyjson_get_str(yyjson_obj_get(eval, "type"));
            if (!typ) typ = yyjson_get_str(yyjson_obj_get(eval, "edge_type"));
            if (typ) edge->type = clean_str_dup(typ);
        }
    }

    yyjson_doc_free(doc);
    return true;
}

/* ── YAML Tactical Spec Parsing ───────────────────────────────────── */

enum YamlSection {
    YAML_SEC_NONE = 0,
    YAML_SEC_NODES,
    YAML_SEC_EDGES
};

static void parse_yaml_tactical_spec(const char *body, SpecCollection *sc) {
    if (!body) return;

    enum YamlSection section = YAML_SEC_NONE;
    bool in_methods = false;
    SpecNode *cur_node = NULL;
    SpecEdge *cur_edge = NULL;

    const char *p = body;
    while (*p) {
        const char *line_start = p;
        while (*p && *p != '\n') p++;
        size_t line_len = (size_t)(p - line_start);
        if (*p == '\n') p++;

        char line[2048];
        if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';

        int indent = 0;
        while (line[indent] == ' ') indent++;
        char *trimmed = line + indent;

        size_t tlen = strlen(trimmed);
        while (tlen > 0 && (trimmed[tlen - 1] == ' ' || trimmed[tlen - 1] == '\t' ||
                            trimmed[tlen - 1] == '\r' || trimmed[tlen - 1] == '\n')) {
            trimmed[--tlen] = '\0';
        }

        if (tlen == 0 || trimmed[0] == '#') continue;

        if (strncmp(trimmed, "nodes:", 6) == 0) {
            section = YAML_SEC_NODES;
            in_methods = false;
            cur_node = NULL;
            continue;
        }
        if (strncmp(trimmed, "edges:", 6) == 0) {
            section = YAML_SEC_EDGES;
            in_methods = false;
            cur_edge = NULL;
            continue;
        }
        if (indent == 0 && (strncmp(trimmed, "domain:", 7) == 0 ||
                            strncmp(trimmed, "bounded_context:", 16) == 0)) {
            section = YAML_SEC_NONE;
            in_methods = false;
            continue;
        }

        if (section == YAML_SEC_NODES) {
            if (strncmp(trimmed, "methods:", 8) == 0) {
                in_methods = true;
                continue;
            }

            if (trimmed[0] == '-' && (trimmed[1] == ' ' || trimmed[1] == '\0')) {
                if (in_methods && indent >= 4) {
                    if (cur_node) {
                        char *m_text = trimmed + 1;
                        while (*m_text == ' ') m_text++;
                        char *clean_m = clean_str_dup(m_text);
                        if (clean_m) {
                            strbuf_append_str(&cur_node->methods, "- ");
                            strbuf_append_str(&cur_node->methods, clean_m);
                            strbuf_append_str(&cur_node->methods, "\n");
                            free(clean_m);
                        }
                    }
                    continue;
                } else {
                    in_methods = false;
                    cur_node = spec_collection_add_node(sc);
                    char *sub = trimmed + 1;
                    while (*sub == ' ') sub++;
                    if (*sub != '\0') {
                        char *col = strchr(sub, ':');
                        if (col) {
                            *col = '\0';
                            char *k = clean_str_dup(sub);
                            char *v = clean_str_dup(col + 1);
                            if (cur_node && k && v) {
                                if (strcmp(k, "symbol") == 0) cur_node->symbol = clean_str_dup(v);
                                else if (strcmp(k, "type") == 0) cur_node->type = clean_str_dup(v);
                                else if (strcmp(k, "target_path") == 0 || strcmp(k, "path") == 0) cur_node->target_path = clean_str_dup(v);
                                else if (strcmp(k, "cbm_uri") == 0 || strcmp(k, "uri") == 0) cur_node->cbm_uri = clean_str_dup(v);
                                else if (strcmp(k, "description") == 0) cur_node->description = clean_str_dup(v);
                            }
                            free(k);
                            free(v);
                        }
                    }
                    continue;
                }
            }

            char *col = strchr(trimmed, ':');
            if (col) {
                *col = '\0';
                char *k = clean_str_dup(trimmed);
                char *v = clean_str_dup(col + 1);
                if (cur_node && k && v) {
                    in_methods = false;
                    if (strcmp(k, "symbol") == 0) {
                        free(cur_node->symbol);
                        cur_node->symbol = clean_str_dup(v);
                    } else if (strcmp(k, "type") == 0) {
                        free(cur_node->type);
                        cur_node->type = clean_str_dup(v);
                    } else if (strcmp(k, "target_path") == 0 || strcmp(k, "path") == 0) {
                        free(cur_node->target_path);
                        cur_node->target_path = clean_str_dup(v);
                    } else if (strcmp(k, "cbm_uri") == 0 || strcmp(k, "uri") == 0) {
                        free(cur_node->cbm_uri);
                        cur_node->cbm_uri = clean_str_dup(v);
                    } else if (strcmp(k, "description") == 0) {
                        free(cur_node->description);
                        cur_node->description = clean_str_dup(v);
                    }
                }
                free(k);
                free(v);
            }
        } else if (section == YAML_SEC_EDGES) {
            if (trimmed[0] == '-' && (trimmed[1] == ' ' || trimmed[1] == '\0')) {
                cur_edge = spec_collection_add_edge(sc);
                char *sub = trimmed + 1;
                while (*sub == ' ') sub++;
                if (*sub != '\0') {
                    char *col = strchr(sub, ':');
                    if (col) {
                        *col = '\0';
                        char *k = clean_str_dup(sub);
                        char *v = clean_str_dup(col + 1);
                        if (cur_edge && k && v) {
                            if (strcmp(k, "source") == 0) cur_edge->source = clean_str_dup(v);
                            else if (strcmp(k, "target") == 0) cur_edge->target = clean_str_dup(v);
                            else if (strcmp(k, "type") == 0 || strcmp(k, "edge_type") == 0) cur_edge->type = clean_str_dup(v);
                        }
                        free(k);
                        free(v);
                    }
                }
                continue;
            }

            char *col = strchr(trimmed, ':');
            if (col) {
                *col = '\0';
                char *k = clean_str_dup(trimmed);
                char *v = clean_str_dup(col + 1);
                if (cur_edge && k && v) {
                    if (strcmp(k, "source") == 0) {
                        free(cur_edge->source);
                        cur_edge->source = clean_str_dup(v);
                    } else if (strcmp(k, "target") == 0) {
                        free(cur_edge->target);
                        cur_edge->target = clean_str_dup(v);
                    } else if (strcmp(k, "type") == 0 || strcmp(k, "edge_type") == 0) {
                        free(cur_edge->type);
                        cur_edge->type = clean_str_dup(v);
                    }
                }
                free(k);
                free(v);
            }
        }
    }
}

/* ── Code Fence Block Scanner ─────────────────────────────────────── */

static bool is_tactical_spec_open_fence(const char *line, size_t *out_fence_len) {
    while (*line == ' ' || *line == '\t') line++;
    size_t fence_len = 0;
    while (line[fence_len] == '`') fence_len++;
    if (fence_len < 3) return false;
    const char *tag = line + fence_len;
    while (*tag == ' ' || *tag == '\t') tag++;
    if (strncasecmp(tag, "tactical-spec", 13) == 0 || strncasecmp(tag, "tactical_spec", 13) == 0) {
        if (out_fence_len) *out_fence_len = fence_len;
        return true;
    }
    return false;
}

static bool is_fence_close(const char *line, size_t min_fence_len) {
    while (*line == ' ' || *line == '\t') line++;
    size_t count = 0;
    while (line[count] == '`') count++;
    if (count < min_fence_len) return false;
    const char *rest = line + count;
    while (*rest == ' ' || *rest == '\t' || *rest == '\r') rest++;
    return (*rest == '\0');
}

static void extract_and_parse_tactical_specs(const char *markdown_text, SpecCollection *sc) {
    if (!markdown_text) return;
    const char *p = markdown_text;
    while (*p) {
        const char *line_start = p;
        while (*p && *p != '\n') p++;
        size_t line_len = (size_t)(p - line_start);
        char line[512];
        if (line_len >= sizeof(line)) line_len = sizeof(line) - 1;
        memcpy(line, line_start, line_len);
        line[line_len] = '\0';
        if (*p == '\n') p++;

        size_t fence_len = 0;
        if (is_tactical_spec_open_fence(line, &fence_len)) {
            StrBuf body;
            strbuf_init(&body);
            while (*p) {
                const char *inner_start = p;
                while (*p && *p != '\n') p++;
                size_t inner_len = (size_t)(p - inner_start);
                char inner_line[512];
                if (inner_len >= sizeof(inner_line)) inner_len = sizeof(inner_line) - 1;
                memcpy(inner_line, inner_start, inner_len);
                inner_line[inner_len] = '\0';
                if (*p == '\n') p++;

                if (is_fence_close(inner_line, fence_len)) {
                    break;
                }
                strbuf_append(&body, inner_start, inner_len);
                strbuf_append_str(&body, "\n");
            }

            if (body.data && body.len > 0) {
                if (!parse_json_tactical_spec(body.data, sc)) {
                    parse_yaml_tactical_spec(body.data, sc);
                }
            }
            strbuf_free(&body);
        }
    }
}

/* ── Edge URI Resolution ─────────────────────────────────────────── */

static void resolve_edge_uri(const char *ref,
                             const SpecCollection *sc,
                             const char *repo_name,
                             const char *norm_file_path,
                             char *out_uri,
                             size_t out_sz) {
    if (!ref || !out_uri || out_sz == 0) return;
    if (strncmp(ref, "cbm://", 6) == 0) {
        snprintf(out_uri, out_sz, "%s", ref);
        return;
    }
    for (size_t i = 0; i < sc->node_count; i++) {
        if (sc->nodes[i].symbol && strcmp(sc->nodes[i].symbol, ref) == 0) {
            if (sc->nodes[i].cbm_uri && sc->nodes[i].cbm_uri[0]) {
                snprintf(out_uri, out_sz, "%s", sc->nodes[i].cbm_uri);
                return;
            }
        }
    }
    snprintf(out_uri, out_sz, "cbm://%s/%s#%s", repo_name ? repo_name : "default",
             norm_file_path ? norm_file_path : "", ref);
}

/* ── Main Compilation & Indexing Entry Point ──────────────────────── */

int cbm_compile_spec_to_horizon(sqlite3 *hdb,
                                const char *origin_horizon,
                                const char *repo_name,
                                const char *file_path,
                                const char *markdown_text,
                                TacticalSpecCompileReport *report,
                                char *err_buf,
                                size_t err_sz) {
    if (report) {
        report->nodes_compiled = 0;
        report->edges_compiled = 0;
        report->sections_indexed = 0;
    }

    if (!hdb || !origin_horizon || !repo_name || !file_path || !markdown_text) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "cbm_compile_spec_to_horizon: invalid NULL parameter");
        }
        return -1;
    }

    char norm_file_path[1024];
    normalize_path(file_path, norm_file_path, sizeof(norm_file_path));

    // Begin atomic immediate transaction
    int rc = sqlite3_exec(hdb, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "Failed to begin transaction: %s", sqlite3_errmsg(hdb));
        }
        return -1;
    }

    int64_t now_epoch = (int64_t)time(NULL);

    // 1. Extract and parse tactical-spec block(s)
    SpecCollection sc;
    spec_collection_init(&sc);
    extract_and_parse_tactical_specs(markdown_text, &sc);

    // 2. Insert tactical-spec nodes
    sqlite3_stmt *stmt_node = NULL;
    const char *sql_node =
        "INSERT OR REPLACE INTO symbolic_nodes "
        "(cbm_uri, label, epistemic_status, is_dangling, code_snippet, created_at) "
        "VALUES (?, ?, 'PROPOSED', 1, ?, ?);";
    rc = sqlite3_prepare_v2(hdb, sql_node, -1, &stmt_node, NULL);
    if (rc != SQLITE_OK) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "Failed to prepare node insert: %s", sqlite3_errmsg(hdb));
        }
        spec_collection_free(&sc);
        sqlite3_exec(hdb, "ROLLBACK;", NULL, NULL, NULL);
        return -1;
    }

    for (size_t i = 0; i < sc.node_count; i++) {
        SpecNode *node = &sc.nodes[i];
        if (!node->symbol || node->symbol[0] == '\0') continue;

        // Compute cbm_uri if not explicitly provided
        if (!node->cbm_uri || node->cbm_uri[0] == '\0') {
            char norm_tgt[1024];
            if (node->target_path && node->target_path[0]) {
                normalize_path(node->target_path, norm_tgt, sizeof(norm_tgt));
            } else {
                snprintf(norm_tgt, sizeof(norm_tgt), "%s", norm_file_path);
            }
            char computed_uri[2048];
            snprintf(computed_uri, sizeof(computed_uri), "cbm://%s/%s#%s", repo_name, norm_tgt, node->symbol);
            free(node->cbm_uri);
            node->cbm_uri = clean_str_dup(computed_uri);
        }

        const char *label = (node->type && node->type[0]) ? node->type : "Symbol";

        // Assemble code_snippet: description + methods
        StrBuf snippet_buf;
        strbuf_init(&snippet_buf);
        if (node->description && node->description[0]) {
            strbuf_append_str(&snippet_buf, node->description);
        }
        if (node->methods.data && node->methods.len > 0) {
            if (snippet_buf.len > 0) strbuf_append_str(&snippet_buf, "\n\nMethods:\n");
            strbuf_append_str(&snippet_buf, node->methods.data);
        }

        sqlite3_reset(stmt_node);
        sqlite3_bind_text(stmt_node, 1, node->cbm_uri, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_node, 2, label, -1, SQLITE_STATIC);
        if (snippet_buf.len > 0) {
            sqlite3_bind_text(stmt_node, 3, snippet_buf.data, -1, SQLITE_STATIC);
        } else {
            sqlite3_bind_null(stmt_node, 3);
        }
        sqlite3_bind_int64(stmt_node, 4, (sqlite3_int64)now_epoch);

        rc = sqlite3_step(stmt_node);
        strbuf_free(&snippet_buf);

        if (rc != SQLITE_DONE) {
            if (err_buf && err_sz > 0) {
                snprintf(err_buf, err_sz, "Failed to insert node %s: %s", node->symbol, sqlite3_errmsg(hdb));
            }
            sqlite3_finalize(stmt_node);
            spec_collection_free(&sc);
            sqlite3_exec(hdb, "ROLLBACK;", NULL, NULL, NULL);
            return -1;
        }

        if (report) report->nodes_compiled++;
    }
    sqlite3_finalize(stmt_node);

    // 3. Insert tactical-spec edges
    sqlite3_stmt *stmt_edge = NULL;
    const char *sql_edge =
        "INSERT OR REPLACE INTO virtual_edges "
        "(source_uri, target_uri, edge_type, origin_horizon, created_at) "
        "VALUES (?, ?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(hdb, sql_edge, -1, &stmt_edge, NULL);
    if (rc != SQLITE_OK) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "Failed to prepare edge insert: %s", sqlite3_errmsg(hdb));
        }
        spec_collection_free(&sc);
        sqlite3_exec(hdb, "ROLLBACK;", NULL, NULL, NULL);
        return -1;
    }

    for (size_t i = 0; i < sc.edge_count; i++) {
        SpecEdge *edge = &sc.edges[i];
        if (!edge->source || !edge->target) continue;

        char src_uri[2048], tgt_uri[2048];
        resolve_edge_uri(edge->source, &sc, repo_name, norm_file_path, src_uri, sizeof(src_uri));
        resolve_edge_uri(edge->target, &sc, repo_name, norm_file_path, tgt_uri, sizeof(tgt_uri));

        const char *edge_type = (edge->type && edge->type[0]) ? edge->type : "REFERENCES";

        sqlite3_reset(stmt_edge);
        sqlite3_bind_text(stmt_edge, 1, src_uri, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_edge, 2, tgt_uri, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_edge, 3, edge_type, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_edge, 4, origin_horizon, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt_edge, 5, (sqlite3_int64)now_epoch);

        rc = sqlite3_step(stmt_edge);
        if (rc != SQLITE_DONE) {
            if (err_buf && err_sz > 0) {
                snprintf(err_buf, err_sz, "Failed to insert edge %s -> %s: %s", src_uri, tgt_uri, sqlite3_errmsg(hdb));
            }
            sqlite3_finalize(stmt_edge);
            spec_collection_free(&sc);
            sqlite3_exec(hdb, "ROLLBACK;", NULL, NULL, NULL);
            return -1;
        }

        if (report) report->edges_compiled++;
    }
    sqlite3_finalize(stmt_edge);
    spec_collection_free(&sc);

    // 4. Clean prior spec_fts and Section symbolic_nodes for idempotency
    sqlite3_stmt *stmt_del_fts = NULL;
    rc = sqlite3_prepare_v2(hdb, "DELETE FROM spec_fts WHERE file_path = ?;", -1, &stmt_del_fts, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt_del_fts, 1, norm_file_path, -1, SQLITE_STATIC);
        sqlite3_step(stmt_del_fts);
        sqlite3_finalize(stmt_del_fts);
    }

    sqlite3_stmt *stmt_del_sec = NULL;
    rc = sqlite3_prepare_v2(hdb, "DELETE FROM symbolic_nodes WHERE label = 'Section' AND cbm_uri LIKE ?;", -1, &stmt_del_sec, NULL);
    if (rc == SQLITE_OK) {
        char sec_pattern[2048];
        snprintf(sec_pattern, sizeof(sec_pattern), "cbm://%s/%s#%%", repo_name, norm_file_path);
        sqlite3_bind_text(stmt_del_sec, 1, sec_pattern, -1, SQLITE_STATIC);
        sqlite3_step(stmt_del_sec);
        sqlite3_finalize(stmt_del_sec);
    }

    // 5. Index Markdown Headings and Section Content
    sqlite3_stmt *stmt_ins_fts = NULL;
    const char *sql_ins_fts = "INSERT INTO spec_fts (file_path, heading_slug, title, content) VALUES (?, ?, ?, ?);";
    rc = sqlite3_prepare_v2(hdb, sql_ins_fts, -1, &stmt_ins_fts, NULL);
    if (rc != SQLITE_OK) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "Failed to prepare spec_fts insert: %s", sqlite3_errmsg(hdb));
        }
        sqlite3_exec(hdb, "ROLLBACK;", NULL, NULL, NULL);
        return -1;
    }

    sqlite3_stmt *stmt_ins_sec = NULL;
    const char *sql_ins_sec =
        "INSERT OR REPLACE INTO symbolic_nodes "
        "(cbm_uri, label, epistemic_status, is_dangling, code_snippet, created_at) "
        "VALUES (?, 'Section', 'PROPOSED', 1, ?, ?);";
    rc = sqlite3_prepare_v2(hdb, sql_ins_sec, -1, &stmt_ins_sec, NULL);
    if (rc != SQLITE_OK) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "Failed to prepare section node insert: %s", sqlite3_errmsg(hdb));
        }
        sqlite3_finalize(stmt_ins_fts);
        sqlite3_exec(hdb, "ROLLBACK;", NULL, NULL, NULL);
        return -1;
    }

    SlugRegistry slug_reg;
    slug_registry_init(&slug_reg);

    bool in_code_block = false;
    bool has_active_section = false;
    char cur_title[256] = {0};
    char cur_slug[128] = {0};
    StrBuf cur_content;
    strbuf_init(&cur_content);

    const char *scan = markdown_text;
    while (*scan) {
        const char *lstart = scan;
        while (*scan && *scan != '\n') scan++;
        size_t llen = (size_t)(scan - lstart);
        char line[2048];
        if (llen >= sizeof(line)) llen = sizeof(line) - 1;
        memcpy(line, lstart, llen);
        line[llen] = '\0';
        if (*scan == '\n') scan++;

        // Track code blocks
        int findent = 0;
        while (line[findent] == ' ') findent++;
        if ((line[findent] == '`' && line[findent + 1] == '`' && line[findent + 2] == '`') ||
            (line[findent] == '~' && line[findent + 1] == '~' && line[findent + 2] == '~')) {
            in_code_block = !in_code_block;
        }

        // Check if heading
        bool is_heading = false;
        char *title_start = NULL;

        if (!in_code_block && findent <= 3) {
            int hcount = 0;
            while (line[findent + hcount] == '#') hcount++;
            if (hcount >= 1 && hcount <= 6 && (line[findent + hcount] == ' ' || line[findent + hcount] == '\t')) {
                is_heading = true;
                title_start = line + findent + hcount;
                while (*title_start == ' ' || *title_start == '\t') title_start++;
                // Trim trailing '#' and whitespace from heading line
                size_t t_len = strlen(title_start);
                while (t_len > 0 && (title_start[t_len - 1] == ' ' || title_start[t_len - 1] == '\t' ||
                                     title_start[t_len - 1] == '\r' || title_start[t_len - 1] == '#')) {
                    title_start[--t_len] = '\0';
                }
            }
        }

        if (is_heading) {
            // Commit previous section if any
            if (has_active_section) {
                char sec_uri[2048];
                snprintf(sec_uri, sizeof(sec_uri), "cbm://%s/%s#%s", repo_name, norm_file_path, cur_slug);

                sqlite3_reset(stmt_ins_fts);
                sqlite3_bind_text(stmt_ins_fts, 1, norm_file_path, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt_ins_fts, 2, cur_slug, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt_ins_fts, 3, cur_title, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt_ins_fts, 4, cur_content.data ? cur_content.data : "", -1, SQLITE_STATIC);
                sqlite3_step(stmt_ins_fts);

                sqlite3_reset(stmt_ins_sec);
                sqlite3_bind_text(stmt_ins_sec, 1, sec_uri, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt_ins_sec, 2, cur_content.data ? cur_content.data : cur_title, -1, SQLITE_STATIC);
                sqlite3_bind_int64(stmt_ins_sec, 3, (sqlite3_int64)now_epoch);
                sqlite3_step(stmt_ins_sec);

                if (report) report->sections_indexed++;
            }

            has_active_section = true;
            snprintf(cur_title, sizeof(cur_title), "%s", title_start);
            slugify_heading(cur_title, cur_slug, sizeof(cur_slug), &slug_reg);
            cur_content.len = 0;
            if (cur_content.data) cur_content.data[0] = '\0';
        } else {
            if (has_active_section) {
                strbuf_append(&cur_content, lstart, llen);
                strbuf_append_str(&cur_content, "\n");
            }
        }
    }

    // Flush last active section
    if (has_active_section) {
        char sec_uri[2048];
        snprintf(sec_uri, sizeof(sec_uri), "cbm://%s/%s#%s", repo_name, norm_file_path, cur_slug);

        sqlite3_reset(stmt_ins_fts);
        sqlite3_bind_text(stmt_ins_fts, 1, norm_file_path, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_ins_fts, 2, cur_slug, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_ins_fts, 3, cur_title, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_ins_fts, 4, cur_content.data ? cur_content.data : "", -1, SQLITE_STATIC);
        sqlite3_step(stmt_ins_fts);

        sqlite3_reset(stmt_ins_sec);
        sqlite3_bind_text(stmt_ins_sec, 1, sec_uri, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt_ins_sec, 2, cur_content.data ? cur_content.data : cur_title, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt_ins_sec, 3, (sqlite3_int64)now_epoch);
        sqlite3_step(stmt_ins_sec);

        if (report) report->sections_indexed++;
    }
    strbuf_free(&cur_content);

    sqlite3_finalize(stmt_ins_fts);
    sqlite3_finalize(stmt_ins_sec);

    rc = sqlite3_exec(hdb, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        if (err_buf && err_sz > 0) {
            snprintf(err_buf, err_sz, "Failed to commit transaction: %s", sqlite3_errmsg(hdb));
        }
        sqlite3_exec(hdb, "ROLLBACK;", NULL, NULL, NULL);
        return -1;
    }

    return 0;
}
