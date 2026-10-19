#include "harness/harness_v2.h"
#include "foundation/sha256.h"
#include "foundation/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

static char *hv2_strdup(const char *s) {
    if (!s) {
        return NULL;
    }
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

typedef struct {
    const char *key;
    yyjson_val *val;
} obj_kv_t;

static int cmp_obj_kv(const void *a, const void *b) {
    const obj_kv_t *ka = (const obj_kv_t *)a;
    const obj_kv_t *kb = (const obj_kv_t *)b;
    return strcmp(ka->key, kb->key);
}

static yyjson_mut_val *canonical_copy_val(yyjson_mut_doc *mdoc, yyjson_val *val, bool *out_oom) {
    if (!val) {
        return yyjson_mut_null(mdoc);
    }
    if (yyjson_is_obj(val)) {
        yyjson_mut_val *mobj = yyjson_mut_obj(mdoc);
        if (!mobj) {
            if (out_oom) *out_oom = true;
            return NULL;
        }
        size_t n = yyjson_obj_size(val);
        if (n == 0) {
            return mobj;
        }
        obj_kv_t *kvs = (obj_kv_t *)malloc(n * sizeof(obj_kv_t));
        if (!kvs) {
            if (out_oom) *out_oom = true;
            return NULL;
        }
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(val, &iter);
        yyjson_val *k = NULL;
        size_t idx = 0;
        while ((k = yyjson_obj_iter_next(&iter)) != NULL && idx < n) {
            kvs[idx].key = yyjson_get_str(k);
            kvs[idx].val = yyjson_obj_iter_get_val(k);
            if (kvs[idx].key) {
                idx++;
            }
        }
        qsort(kvs, idx, sizeof(obj_kv_t), cmp_obj_kv);
        for (size_t i = 0; i < idx; i++) {
            yyjson_mut_val *mk = yyjson_mut_strcpy(mdoc, kvs[i].key);
            yyjson_mut_val *mv = canonical_copy_val(mdoc, kvs[i].val, out_oom);
            if (!mk || !mv || (out_oom && *out_oom)) {
                free(kvs);
                if (out_oom) *out_oom = true;
                return NULL;
            }
            yyjson_mut_obj_add(mobj, mk, mv);
        }
        free(kvs);
        return mobj;
    }
    if (yyjson_is_arr(val)) {
        yyjson_mut_val *marr = yyjson_mut_arr(mdoc);
        if (!marr) {
            if (out_oom) *out_oom = true;
            return NULL;
        }
        yyjson_arr_iter iter;
        yyjson_arr_iter_init(val, &iter);
        yyjson_val *elem = NULL;
        while ((elem = yyjson_arr_iter_next(&iter)) != NULL) {
            yyjson_mut_val *melem = canonical_copy_val(mdoc, elem, out_oom);
            if (!melem || (out_oom && *out_oom)) {
                if (out_oom) *out_oom = true;
                return NULL;
            }
            yyjson_mut_arr_append(marr, melem);
        }
        return marr;
    }
    yyjson_mut_val *copied = yyjson_val_mut_copy(mdoc, val);
    if (!copied && out_oom) {
        *out_oom = true;
    }
    return copied;
}

int cbm_canonical_json_from_val(yyjson_val *val, char **out_canonical) {
    if (!val || !out_canonical) {
        return -1;
    }
    *out_canonical = NULL;
    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    if (!mdoc) {
        return -1;
    }
    bool oom = false;
    yyjson_mut_val *mroot = canonical_copy_val(mdoc, val, &oom);
    if (oom || !mroot) {
        yyjson_mut_doc_free(mdoc);
        return -1;
    }
    yyjson_mut_doc_set_root(mdoc, mroot);
    size_t len = 0;
    char *s = yyjson_mut_write(mdoc, 0, &len);
    yyjson_mut_doc_free(mdoc);
    if (!s) {
        return -1;
    }
    *out_canonical = s;
    return 0;
}

int cbm_canonical_json(const char *json_str, char **out_canonical) {
    if (!json_str || !out_canonical) {
        return -1;
    }
    *out_canonical = NULL;
    yyjson_doc *doc = yyjson_read(json_str, strlen(json_str), 0);
    if (!doc) {
        return -1;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    int rc = cbm_canonical_json_from_val(root, out_canonical);
    yyjson_doc_free(doc);
    return rc;
}

int cbm_canonical_json_sha256(const char *json_str, char out_hex[65]) {
    if (!json_str || !out_hex) {
        return -1;
    }
    char *canon = NULL;
    if (cbm_canonical_json(json_str, &canon) != 0 || !canon) {
        return -1;
    }
    cbm_sha256_hex(canon, strlen(canon), out_hex);
    free(canon);
    return 0;
}

bool cbm_harness_validate_sha256_hex(const char *hex_str) {
    if (!hex_str) {
        return false;
    }
    size_t len = strlen(hex_str);
    if (len != 64) {
        return false;
    }
    for (size_t i = 0; i < 64; i++) {
        char c = hex_str[i];
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!ok) {
            return false;
        }
    }
    return true;
}

bool cbm_harness_validate_span(int start_line, int end_line) {
    return (start_line >= 1 && end_line >= start_line);
}

bool cbm_harness_validate_k_hop(int k_hop) {
    return (k_hop >= 1 && k_hop <= 3);
}

int cbm_harness_compute_snapshot_id(const char *graph_generation,
                                    const char *rules_revision,
                                    const char *coverage_revision,
                                    char out_snapshot_id[65]) {
    if (!graph_generation || !graph_generation[0] ||
        !rules_revision || !rules_revision[0] ||
        !coverage_revision || !coverage_revision[0] ||
        !out_snapshot_id) {
        return -1;
    }
    /* Canonical JSON tuple with extractor_version and component keys (003 §2.3) */
    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    if (!mdoc) {
        return -1;
    }
    yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
    yyjson_mut_doc_set_root(mdoc, mroot);
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "coverage_revision", coverage_revision);
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "extractor_version", "2.0.0");
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "graph_generation", graph_generation);
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "rules_revision", rules_revision);
    size_t len = 0;
    char *s = yyjson_mut_write(mdoc, 0, &len);
    yyjson_mut_doc_free(mdoc);
    if (!s) {
        return -1;
    }
    int rc = cbm_canonical_json_sha256(s, out_snapshot_id);
    free(s);
    return rc;
}

cbm_effect_tristate_t cbm_effect_join(cbm_effect_tristate_t a, cbm_effect_tristate_t b) {
    /* Dominance order: PRESENT (2) > UNKNOWN (1) > ABSENT (0) */
    return (a >= b) ? a : b;
}

const char *cbm_effect_tristate_str(cbm_effect_tristate_t st) {
    switch (st) {
    case CBM_EFFECT_PRESENT:
        return "present";
    case CBM_EFFECT_UNKNOWN:
        return "unknown";
    case CBM_EFFECT_ABSENT:
    default:
        return "absent";
    }
}

cbm_effect_tristate_t cbm_effect_tristate_parse(const char *str) {
    if (!str) {
        return CBM_EFFECT_UNKNOWN;
    }
    if (strcmp(str, "present") == 0) {
        return CBM_EFFECT_PRESENT;
    }
    if (strcmp(str, "absent") == 0) {
        return CBM_EFFECT_ABSENT;
    }
    return CBM_EFFECT_UNKNOWN;
}

bool cbm_effect_has_any_unknown(const cbm_effect_fingerprint_t *fp) {
    if (!fp) {
        return true;
    }
    return fp->writes_db == CBM_EFFECT_UNKNOWN ||
           fp->network_io == CBM_EFFECT_UNKNOWN ||
           fp->fs_mutation == CBM_EFFECT_UNKNOWN ||
           fp->spawns_process == CBM_EFFECT_UNKNOWN ||
           fp->global_state_write == CBM_EFFECT_UNKNOWN ||
           fp->throws_or_panics == CBM_EFFECT_UNKNOWN ||
           fp->dynamic_dispatch == CBM_EFFECT_UNKNOWN;
}

bool cbm_effect_has_any_side_effect(const cbm_effect_fingerprint_t *fp) {
    if (!fp) {
        return true;
    }
    return fp->writes_db != CBM_EFFECT_ABSENT ||
           fp->network_io != CBM_EFFECT_ABSENT ||
           fp->fs_mutation != CBM_EFFECT_ABSENT ||
           fp->spawns_process != CBM_EFFECT_ABSENT ||
           fp->global_state_write != CBM_EFFECT_ABSENT ||
           fp->throws_or_panics != CBM_EFFECT_ABSENT ||
           fp->dynamic_dispatch != CBM_EFFECT_ABSENT;
}

void cbm_effect_fingerprint_free(cbm_effect_fingerprint_t *fp) {
    if (!fp) {
        return;
    }
    free(fp->uncertainty_reasons_json);
    memset(fp, 0, sizeof(*fp));
}

typedef struct {
    char **items;
    int count;
    int cap;
} str_set_t;

static void str_set_add(str_set_t *set, const char *s) {
    if (!set || !s || !s[0]) {
        return;
    }
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->items[i], s) == 0) {
            return;
        }
    }
    if (set->count >= set->cap) {
        int ncap = (set->cap == 0) ? 8 : set->cap * 2;
        char **nitems = (char **)realloc(set->items, (size_t)ncap * sizeof(char *));
        if (!nitems) {
            return;
        }
        set->items = nitems;
        set->cap = ncap;
    }
    set->items[set->count++] = hv2_strdup(s);
}

static int cmp_str_ptr(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

static char *str_set_to_json(str_set_t *set) {
    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *marr = yyjson_mut_arr(mdoc);
    yyjson_mut_doc_set_root(mdoc, marr);
    if (set && set->count > 0) {
        qsort(set->items, (size_t)set->count, sizeof(char *), cmp_str_ptr);
        for (int i = 0; i < set->count; i++) {
            yyjson_mut_arr_add_strcpy(mdoc, marr, set->items[i]);
        }
    }
    size_t len = 0;
    char *s = yyjson_mut_write(mdoc, 0, &len);
    yyjson_mut_doc_free(mdoc);
    return s ? s : hv2_strdup("[]");
}

static void str_set_free(str_set_t *set) {
    if (!set) {
        return;
    }
    for (int i = 0; i < set->count; i++) {
        free(set->items[i]);
    }
    free(set->items);
    memset(set, 0, sizeof(*set));
}

/* Read lines [start_line, end_line] (1-indexed, inclusive) from root_path + "/" + rel_path.
 * Caller must free() returned string. */
static char *read_file_span(const char *root_path, const char *rel_path,
                            int start_line, int end_line) {
    if (!rel_path || !rel_path[0]) {
        return NULL;
    }
    char full_path[2048];
    if (root_path && root_path[0] && rel_path[0] != '/' &&
        !(isalpha((unsigned char)rel_path[0]) && rel_path[1] == ':')) {
        snprintf(full_path, sizeof(full_path), "%s/%s", root_path, rel_path);
    } else {
        snprintf(full_path, sizeof(full_path), "%s", rel_path);
    }

    FILE *fp = fopen(full_path, "rb");
    if (!fp) {
        return NULL;
    }

    if (start_line < 1) {
        start_line = 1;
    }
    if (end_line < start_line) {
        end_line = start_line + 100;
    }

    size_t cap = 1024;
    size_t len = 0;
    char *out = (char *)malloc(cap);
    if (!out) {
        fclose(fp);
        return NULL;
    }
    out[0] = '\0';

    char line_buf[4096];
    int cur_line = 1;
    while (fgets(line_buf, sizeof(line_buf), fp) != NULL) {
        if (cur_line >= start_line && cur_line <= end_line) {
            size_t l = strlen(line_buf);
            if (len + l + 1 >= cap) {
                cap = (cap + l + 1) * 2;
                char *nout = (char *)realloc(out, cap);
                if (!nout) {
                    free(out);
                    fclose(fp);
                    return NULL;
                }
                out = nout;
            }
            memcpy(out + len, line_buf, l);
            len += l;
            out[len] = '\0';
        }
        if (strchr(line_buf, '\n') != NULL) {
            if (cur_line >= end_line) {
                break;
            }
            cur_line++;
        }
    }
    fclose(fp);
    return out;
}

static bool contains_token_ci(const char *haystack, const char *needle) {
    if (!haystack || !needle || !needle[0]) {
        return false;
    }
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++) {
#ifdef _WIN32
        if (_strnicmp(p, needle, nlen) == 0) {
            return true;
        }
#else
        if (strncasecmp(p, needle, nlen) == 0) {
            return true;
        }
#endif
    }
    return false;
}

/* Classify intrinsic effects for a single node based on properties, source text, and outgoing edges. */
static void classify_intrinsic_node(cbm_store_t *store,
                                    const char *root_path,
                                    const cbm_node_t *node,
                                    cbm_effect_fingerprint_t *fp,
                                    str_set_t *reasons) {
    memset(fp, 0, sizeof(*fp));

    /* Check explicit properties_json overrides or hints if present */
    if (node->properties_json && node->properties_json[0]) {
        yyjson_doc *pdoc = yyjson_read(node->properties_json, strlen(node->properties_json), 0);
        if (pdoc) {
            yyjson_val *proot = yyjson_doc_get_root(pdoc);
            if (proot && yyjson_is_obj(proot)) {
                yyjson_val *eff = yyjson_obj_get(proot, "effects");
                if (eff && yyjson_is_obj(eff)) {
                    yyjson_val *v;
                    if ((v = yyjson_obj_get(eff, "writes_db")) && yyjson_is_str(v))
                        fp->writes_db = cbm_effect_tristate_parse(yyjson_get_str(v));
                    if ((v = yyjson_obj_get(eff, "network_io")) && yyjson_is_str(v))
                        fp->network_io = cbm_effect_tristate_parse(yyjson_get_str(v));
                    if ((v = yyjson_obj_get(eff, "fs_mutation")) && yyjson_is_str(v))
                        fp->fs_mutation = cbm_effect_tristate_parse(yyjson_get_str(v));
                    if ((v = yyjson_obj_get(eff, "spawns_process")) && yyjson_is_str(v))
                        fp->spawns_process = cbm_effect_tristate_parse(yyjson_get_str(v));
                    if ((v = yyjson_obj_get(eff, "global_state_write")) && yyjson_is_str(v))
                        fp->global_state_write = cbm_effect_tristate_parse(yyjson_get_str(v));
                    if ((v = yyjson_obj_get(eff, "throws_or_panics")) && yyjson_is_str(v))
                        fp->throws_or_panics = cbm_effect_tristate_parse(yyjson_get_str(v));
                    if ((v = yyjson_obj_get(eff, "dynamic_dispatch")) && yyjson_is_str(v))
                        fp->dynamic_dispatch = cbm_effect_tristate_parse(yyjson_get_str(v));
                    yyjson_val *urs = yyjson_obj_get(eff, "uncertainty_reasons");
                    if (urs && yyjson_is_arr(urs)) {
                        yyjson_arr_iter it;
                        yyjson_arr_iter_init(urs, &it);
                        yyjson_val *ur;
                        while ((ur = yyjson_arr_iter_next(&it)) != NULL) {
                            if (yyjson_is_str(ur)) {
                                str_set_add(reasons, yyjson_get_str(ur));
                            }
                        }
                    }
                }
                yyjson_val *unres = yyjson_obj_get(proot, "unresolved_call");
                if (unres && yyjson_get_bool(unres)) {
                    fp->dynamic_dispatch = cbm_effect_join(fp->dynamic_dispatch, CBM_EFFECT_UNKNOWN);
                    str_set_add(reasons, "unresolved_call_target");
                }
            }
            yyjson_doc_free(pdoc);
        }
    }

    /* Inspect source span if available */
    char *code = read_file_span(root_path, node->file_path, node->start_line, node->end_line);
    const char *text_to_scan = code ? code : (node->properties_json ? node->properties_json : "");

    if (text_to_scan && text_to_scan[0]) {
        if (contains_token_ci(text_to_scan, "sqlite3_exec") ||
            contains_token_ci(text_to_scan, "sqlite3_step") ||
            contains_token_ci(text_to_scan, "INSERT INTO") ||
            contains_token_ci(text_to_scan, "UPDATE ") ||
            contains_token_ci(text_to_scan, "DELETE FROM") ||
            contains_token_ci(text_to_scan, ".execute(") ||
            contains_token_ci(text_to_scan, "db.commit")) {
            fp->writes_db = cbm_effect_join(fp->writes_db, CBM_EFFECT_PRESENT);
        }
        if (contains_token_ci(text_to_scan, "socket(") ||
            contains_token_ci(text_to_scan, "connect(") ||
            contains_token_ci(text_to_scan, "curl_") ||
            contains_token_ci(text_to_scan, "http.") ||
            contains_token_ci(text_to_scan, "fetch(") ||
            contains_token_ci(text_to_scan, "requests.")) {
            fp->network_io = cbm_effect_join(fp->network_io, CBM_EFFECT_PRESENT);
        }
        if (contains_token_ci(text_to_scan, "fwrite(") ||
            contains_token_ci(text_to_scan, "unlink(") ||
            contains_token_ci(text_to_scan, "rename(") ||
            contains_token_ci(text_to_scan, "write_file") ||
            contains_token_ci(text_to_scan, "os.remove")) {
            fp->fs_mutation = cbm_effect_join(fp->fs_mutation, CBM_EFFECT_PRESENT);
        }
        if (contains_token_ci(text_to_scan, "popen(") ||
            contains_token_ci(text_to_scan, "fork(") ||
            contains_token_ci(text_to_scan, "execve(") ||
            contains_token_ci(text_to_scan, "CreateProcess") ||
            contains_token_ci(text_to_scan, "subprocess.")) {
            fp->spawns_process = cbm_effect_join(fp->spawns_process, CBM_EFFECT_PRESENT);
        }
        if (contains_token_ci(text_to_scan, "setenv(") ||
            contains_token_ci(text_to_scan, "_putenv") ||
            contains_token_ci(text_to_scan, "global ")) {
            fp->global_state_write = cbm_effect_join(fp->global_state_write, CBM_EFFECT_PRESENT);
        }
        if (contains_token_ci(text_to_scan, "abort(") ||
            contains_token_ci(text_to_scan, "panic(") ||
            contains_token_ci(text_to_scan, "raise ") ||
            contains_token_ci(text_to_scan, "throw ")) {
            fp->throws_or_panics = cbm_effect_join(fp->throws_or_panics, CBM_EFFECT_PRESENT);
        }
        if (contains_token_ci(text_to_scan, "dlopen(") ||
            contains_token_ci(text_to_scan, "dlsym(") ||
            contains_token_ci(text_to_scan, "getattr(") ||
            contains_token_ci(text_to_scan, "eval(")) {
            fp->dynamic_dispatch = cbm_effect_join(fp->dynamic_dispatch, CBM_EFFECT_PRESENT);
            fp->writes_db = cbm_effect_join(fp->writes_db, CBM_EFFECT_UNKNOWN);
            fp->network_io = cbm_effect_join(fp->network_io, CBM_EFFECT_UNKNOWN);
            fp->fs_mutation = cbm_effect_join(fp->fs_mutation, CBM_EFFECT_UNKNOWN);
            str_set_add(reasons, "dynamic_dispatch_or_reflection");
        }
    }
    free(code);

    /* Inspect outgoing edges for unresolved/external/dynamic indicators */
    if (store && node->id > 0) {
        cbm_edge_t *edges = NULL;
        int edge_count = 0;
        if (cbm_store_find_edges_by_source(store, node->id, &edges, &edge_count) == CBM_STORE_OK) {
            for (int i = 0; i < edge_count; i++) {
                if (edges[i].properties_json &&
                    (contains_token_ci(edges[i].properties_json, "\"unresolved\":true") ||
                     contains_token_ci(edges[i].properties_json, "\"dynamic\":true"))) {
                    fp->dynamic_dispatch = cbm_effect_join(fp->dynamic_dispatch, CBM_EFFECT_UNKNOWN);
                    str_set_add(reasons, "unresolved_edge_target");
                }
            }
            cbm_store_free_edges(edges, edge_count);
        }
    }
}

static bool join_fingerprint_into(cbm_effect_fingerprint_t *dst,
                                  const cbm_effect_fingerprint_t *src) {
    bool changed = false;
    cbm_effect_tristate_t v;

    v = cbm_effect_join(dst->writes_db, src->writes_db);
    if (v != dst->writes_db) { dst->writes_db = v; changed = true; }

    v = cbm_effect_join(dst->network_io, src->network_io);
    if (v != dst->network_io) { dst->network_io = v; changed = true; }

    v = cbm_effect_join(dst->fs_mutation, src->fs_mutation);
    if (v != dst->fs_mutation) { dst->fs_mutation = v; changed = true; }

    v = cbm_effect_join(dst->spawns_process, src->spawns_process);
    if (v != dst->spawns_process) { dst->spawns_process = v; changed = true; }

    v = cbm_effect_join(dst->global_state_write, src->global_state_write);
    if (v != dst->global_state_write) { dst->global_state_write = v; changed = true; }

    v = cbm_effect_join(dst->throws_or_panics, src->throws_or_panics);
    if (v != dst->throws_or_panics) { dst->throws_or_panics = v; changed = true; }

    v = cbm_effect_join(dst->dynamic_dispatch, src->dynamic_dispatch);
    if (v != dst->dynamic_dispatch) { dst->dynamic_dispatch = v; changed = true; }

    return changed;
}

/* Cycle-safe fixed-point aggregation across outbound CALLS subgraph (U07, U08, F01). */
int cbm_harness_compute_node_effects(cbm_store_t *store,
                                     const char *project,
                                     int64_t node_id,
                                     int max_depth,
                                     cbm_effect_fingerprint_t *out_fp) {
    if (!store || !out_fp || node_id <= 0) {
        return -1;
    }
    memset(out_fp, 0, sizeof(*out_fp));
    if (max_depth < 1) {
        max_depth = 3;
    }

    cbm_project_t proj = {0};
    const char *root_path = NULL;
    if (project && cbm_store_get_project(store, project, &proj) == CBM_STORE_OK) {
        root_path = proj.root_path;
    }

    cbm_node_t start_node = {0};
    if (cbm_store_find_node_by_id(store, node_id, &start_node) != CBM_STORE_OK) {
        cbm_project_free_fields(&proj);
        out_fp->writes_db = CBM_EFFECT_UNKNOWN;
        out_fp->uncertainty_reasons_json = hv2_strdup("[\"missing_node_in_store\"]");
        return 0;
    }

    str_set_t reasons = {0};
    classify_intrinsic_node(store, root_path, &start_node, out_fp, &reasons);

    /* Traverse outbound CALLS up to max_depth with cycle detection */
    const char *edge_types[] = {"CALLS"};
    cbm_traverse_result_t trav = {0};
    if (cbm_store_bfs(store, node_id, "outbound", edge_types, 1, max_depth, 128, &trav) == CBM_STORE_OK) {
        for (int i = 0; i < trav.visited_count; i++) {
            cbm_effect_fingerprint_t child_fp = {0};
            classify_intrinsic_node(store, root_path, &trav.visited[i].node, &child_fp, &reasons);
            join_fingerprint_into(out_fp, &child_fp);
            cbm_effect_fingerprint_free(&child_fp);
        }
        cbm_store_traverse_free(&trav);
    }

    cbm_node_free_fields(&start_node);
    cbm_project_free_fields(&proj);

    out_fp->uncertainty_reasons_json = str_set_to_json(&reasons);
    str_set_free(&reasons);
    return 0;
}

int cbm_harness_compute_coverage_revision(cbm_store_t *store,
                                          const char *project,
                                          const char *const *target_files,
                                          int target_file_count,
                                          char out_coverage_rev[65],
                                          bool *out_coverage_complete,
                                          char **out_coverage_digest_json) {
    if (out_coverage_complete) {
        *out_coverage_complete = true;
    }
    if (out_coverage_digest_json) {
        *out_coverage_digest_json = NULL;
    }
    if (!store || !project || !out_coverage_rev) {
        return -1;
    }

    cbm_coverage_row_t *rows = NULL;
    int row_count = 0;
    cbm_store_coverage_get(store, project, &rows, &row_count);

    bool complete = true;
    int gap_count = 0;

    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
    yyjson_mut_doc_set_root(mdoc, mroot);

    yyjson_mut_val *gaps_arr = yyjson_mut_arr(mdoc);
    yyjson_mut_val *hashes_arr = yyjson_mut_arr(mdoc);

    /* Check recorded coverage rows (every row in index_coverage represents a coverage gap/skip) */
    for (int i = 0; i < row_count; i++) {
        bool affects_target = (target_file_count == 0);
        for (int j = 0; j < target_file_count; j++) {
            if (target_files[j] && rows[i].rel_path &&
                strcmp(target_files[j], rows[i].rel_path) == 0) {
                affects_target = true;
                break;
            }
        }
        if (!affects_target) {
            continue;
        }
        const char *knd = rows[i].kind ? rows[i].kind : "unknown";
        if (strcmp(knd, "indexed") != 0 && strcmp(knd, "clean") != 0 &&
            strcmp(knd, "no_recorded_issue") != 0) {
            complete = false;
            gap_count++;
            yyjson_mut_val *gobj = yyjson_mut_obj(mdoc);
            yyjson_mut_obj_add_strcpy(mdoc, gobj, "path", rows[i].rel_path ? rows[i].rel_path : "");
            yyjson_mut_obj_add_strcpy(mdoc, gobj, "reason", rows[i].detail ? rows[i].detail : "coverage_gap");
            yyjson_mut_obj_add_strcpy(mdoc, gobj, "status", knd);
            yyjson_mut_arr_append(gaps_arr, gobj);
        }
    }
    cbm_store_free_coverage(rows, row_count);

    /* Include file hashes for target files */
    for (int j = 0; j < target_file_count; j++) {
        if (!target_files[j] || !target_files[j][0]) {
            continue;
        }
        cbm_file_hash_t fh = {0};
        if (cbm_store_get_file_hash(store, project, target_files[j], &fh) == CBM_STORE_OK) {
            yyjson_mut_val *hobj = yyjson_mut_obj(mdoc);
            yyjson_mut_obj_add_strcpy(mdoc, hobj, "path", target_files[j]);
            yyjson_mut_obj_add_strcpy(mdoc, hobj, "sha256", fh.sha256 ? fh.sha256 : "");
            yyjson_mut_arr_append(hashes_arr, hobj);
            cbm_store_clear_file_hash(&fh);
        }
    }

    yyjson_mut_obj_add_bool(mdoc, mroot, "coverage_complete", complete);
    yyjson_mut_obj_add_int(mdoc, mroot, "gap_count", gap_count);
    yyjson_mut_obj_add_val(mdoc, mroot, "gaps", gaps_arr);
    yyjson_mut_obj_add_val(mdoc, mroot, "file_hashes", hashes_arr);

    size_t raw_len = 0;
    char *raw_json = yyjson_mut_write(mdoc, 0, &raw_len);
    yyjson_mut_doc_free(mdoc);
    if (!raw_json) {
        return -1;
    }

    char *canon = NULL;
    if (cbm_canonical_json(raw_json, &canon) != 0 || !canon) {
        free(raw_json);
        return -1;
    }
    free(raw_json);

    cbm_sha256_hex(canon, strlen(canon), out_coverage_rev);
    if (out_coverage_complete) {
        *out_coverage_complete = complete;
    }
    if (out_coverage_digest_json) {
        *out_coverage_digest_json = canon;
    } else {
        free(canon);
    }
    return 0;
}

/* Synthesize concise skeleton signature from source code or node name + label */
static char *synthesize_skeleton_signature(const cbm_node_t *node, const char *code_span) {
    if (code_span && code_span[0]) {
        /* Extract first non-empty line up to '{' or newline */
        const char *p = code_span;
        while (*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
            p++;
        }
        const char *end = p;
        while (*end && *end != '\r' && *end != '\n' && *end != '{') {
            end++;
        }
        size_t len = (size_t)(end - p);
        while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) {
            len--;
        }
        if (len > 0 && len < 256) {
            char *sig = (char *)malloc(len + 1);
            if (sig) {
                memcpy(sig, p, len);
                sig[len] = '\0';
                return sig;
            }
        }
    }
    const char *label = node->label ? node->label : "Symbol";
    const char *name = node->name ? node->name : (node->qualified_name ? node->qualified_name : "unknown");
    size_t sz = strlen(label) + strlen(name) + 16;
    char *sig = (char *)malloc(sz);
    if (sig) {
        snprintf(sig, sz, "%s %s(...)", label, name);
    }
    return sig;
}

static void add_effect_fingerprint_to_json(yyjson_mut_doc *mdoc,
                                           yyjson_mut_val *parent_obj,
                                           const cbm_effect_fingerprint_t *fp) {
    yyjson_mut_val *eobj = yyjson_mut_obj(mdoc);
    yyjson_mut_obj_add_strcpy(mdoc, eobj, "dynamic_dispatch", cbm_effect_tristate_str(fp->dynamic_dispatch));
    yyjson_mut_obj_add_strcpy(mdoc, eobj, "fs_mutation", cbm_effect_tristate_str(fp->fs_mutation));
    yyjson_mut_obj_add_strcpy(mdoc, eobj, "global_state_write", cbm_effect_tristate_str(fp->global_state_write));
    yyjson_mut_obj_add_strcpy(mdoc, eobj, "network_io", cbm_effect_tristate_str(fp->network_io));
    yyjson_mut_obj_add_strcpy(mdoc, eobj, "spawns_process", cbm_effect_tristate_str(fp->spawns_process));
    yyjson_mut_obj_add_strcpy(mdoc, eobj, "throws_or_panics", cbm_effect_tristate_str(fp->throws_or_panics));
    yyjson_mut_obj_add_strcpy(mdoc, eobj, "writes_db", cbm_effect_tristate_str(fp->writes_db));

    yyjson_doc *udoc = fp->uncertainty_reasons_json
                           ? yyjson_read(fp->uncertainty_reasons_json, strlen(fp->uncertainty_reasons_json), 0)
                           : NULL;
    if (udoc && yyjson_doc_get_root(udoc)) {
        yyjson_mut_obj_add_val(mdoc, eobj, "uncertainty_reasons",
                               yyjson_val_mut_copy(mdoc, yyjson_doc_get_root(udoc)));
    } else {
        yyjson_mut_obj_add_val(mdoc, eobj, "uncertainty_reasons", yyjson_mut_arr(mdoc));
    }
    if (udoc) {
        yyjson_doc_free(udoc);
    }
    yyjson_mut_obj_add_val(mdoc, parent_obj, "effects", eobj);
}

typedef struct {
    cbm_node_t node;
    int hop_distance;
    cbm_effect_fingerprint_t effects;
    char *code_span;
    char *signature;
} extracted_pole_t;

static void free_extracted_poles(extracted_pole_t *poles, int count) {
    if (!poles) {
        return;
    }
    for (int i = 0; i < count; i++) {
        cbm_node_free_fields(&poles[i].node);
        cbm_effect_fingerprint_free(&poles[i].effects);
        free(poles[i].code_span);
        free(poles[i].signature);
    }
    free(poles);
}

static bool is_fast_path_change_kind(const char *change_kind) {
    if (!change_kind || !change_kind[0]) {
        return false;
    }
    return strcmp(change_kind, "pure_refactor") == 0 ||
           strcmp(change_kind, "local_fix") == 0 ||
           strcmp(change_kind, "localized_mutation") == 0 ||
           strcmp(change_kind, "test_only") == 0;
}

static bool glob_matches_path(const char *glob, const char *path) {
    if (!glob || !glob[0] || strcmp(glob, "*") == 0 || strcmp(glob, "**") == 0 || strcmp(glob, "**/*") == 0) {
        return true;
    }
    if (!path || !path[0]) {
        return false;
    }
    /* Prefix match before '*' */
    const char *star = strchr(glob, '*');
    if (!star) {
        return strcmp(glob, path) == 0;
    }
    size_t prefix_len = (size_t)(star - glob);
    return strncmp(glob, path, prefix_len) == 0;
}

int cbm_harness_extract_bundle(cbm_store_t *store,
                               cbm_gov_store_t *gov_store,
                               const char *project,
                               const char *task_id,
                               const char *change_kind,
                               const char *const *seed_poles,
                               int seed_pole_count,
                               int k_hop,
                               int64_t evaluation_time,
                               char **out_bundle_json,
                               char out_bundle_hash[65],
                               char out_snapshot_id[65]) {
    if (!store || !gov_store || !project || !seed_poles || seed_pole_count <= 0 || !out_bundle_json) {
        return -1;
    }
    *out_bundle_json = NULL;
    if (!cbm_harness_validate_k_hop(k_hop)) {
        return -2;
    }
    if (evaluation_time <= 0) {
        evaluation_time = (int64_t)time(NULL);
    }

    /* 1. Evaluate active tradition rules and rules_revision */
    cbm_tradition_rule_t *active_rules = NULL;
    int active_rule_count = 0;
    char rules_rev[65] = {0};
    char *active_rules_json = NULL;
    cbm_gov_status_t gov_st = cbm_gov_evaluate_active_rules(
        gov_store, evaluation_time, &active_rules, &active_rule_count, rules_rev, &active_rules_json);
    if (gov_st != CBM_GOV_OK) {
        return (int)gov_st;
    }

    /* 2. Collect seed nodes (hop 0) and k-hop neighbors (hop >= 1) */
    cbm_project_t proj = {0};
    const char *root_path = NULL;
    if (cbm_store_get_project(store, project, &proj) == CBM_STORE_OK) {
        root_path = proj.root_path;
    }

    int pole_cap = 16;
    int pole_count = 0;
    extracted_pole_t *poles = (extracted_pole_t *)calloc((size_t)pole_cap, sizeof(extracted_pole_t));
    if (!poles) {
        cbm_gov_rules_free(active_rules, active_rule_count);
        free(active_rules_json);
        cbm_project_free_fields(&proj);
        return -1;
    }

    for (int i = 0; i < seed_pole_count; i++) {
        if (!seed_poles[i] || !seed_poles[i][0]) {
            continue;
        }
        cbm_node_t n = {0};
        if (cbm_store_find_node_by_qn(store, project, seed_poles[i], &n) != CBM_STORE_OK) {
            /* Try searching by name if full QN was not passed */
            cbm_search_params_t sp = {0};
            sp.project = project;
            sp.name_pattern = seed_poles[i];
            sp.limit = 1;
            cbm_search_output_t sout = {0};
            if (cbm_store_search(store, &sp, &sout) == CBM_STORE_OK && sout.count > 0) {
                cbm_store_find_node_by_id(store, sout.results[0].node.id, &n);
            }
            cbm_store_search_free(&sout);
        }
        if (n.id <= 0) {
            continue;
        }

        /* Avoid duplicate seeds */
        bool exists = false;
        for (int p = 0; p < pole_count; p++) {
            if (poles[p].node.id == n.id) {
                exists = true;
                break;
            }
        }
        if (exists) {
            cbm_node_free_fields(&n);
            continue;
        }

        if (pole_count >= pole_cap) {
            int ncap = pole_cap * 2;
            extracted_pole_t *npoles = (extracted_pole_t *)realloc(
                poles, (size_t)ncap * sizeof(extracted_pole_t));
            if (!npoles) {
                cbm_node_free_fields(&n);
                free_extracted_poles(poles, pole_count);
                cbm_gov_rules_free(active_rules, active_rule_count);
                free(active_rules_json);
                cbm_project_free_fields(&proj);
                return -1;
            }
            poles = npoles;
            pole_cap = ncap;
        }
        memset(&poles[pole_count], 0, sizeof(extracted_pole_t));
        poles[pole_count].node = n;
        poles[pole_count].hop_distance = 0;
        cbm_harness_compute_node_effects(store, project, n.id, k_hop, &poles[pole_count].effects);
        poles[pole_count].code_span = read_file_span(root_path, n.file_path, n.start_line, n.end_line);
        if (!poles[pole_count].code_span) {
            /* Fallback synthetic body when source file is not on disk (e.g. unit test store) */
            size_t bsz = strlen(n.qualified_name ? n.qualified_name : "sym") + 64;
            poles[pole_count].code_span = (char *)malloc(bsz);
            if (poles[pole_count].code_span) {
                snprintf(poles[pole_count].code_span, bsz, "/* integral body for %s */\nvoid %s(void) {}",
                         n.qualified_name ? n.qualified_name : "sym",
                         n.name ? n.name : "sym");
            }
        }
        poles[pole_count].signature = synthesize_skeleton_signature(&n, poles[pole_count].code_span);
        pole_count++;
    }

    int seed_resolved_count = pole_count;

    /* Expand k-hop neighborhood around each seed pole */
    const char *bfs_edges[] = {"CALLS", "IMPORTS", "REFERENCES"};
    for (int s = 0; s < seed_resolved_count; s++) {
        cbm_traverse_result_t trav = {0};
        if (cbm_store_bfs(store, poles[s].node.id, "both", bfs_edges, 3, k_hop, 64, &trav) == CBM_STORE_OK) {
            for (int v = 0; v < trav.visited_count; v++) {
                int64_t nid = trav.visited[v].node.id;
                int hop = trav.visited[v].hop;
                if (hop < 1) {
                    hop = 1;
                }
                bool seen = false;
                for (int p = 0; p < pole_count; p++) {
                    if (poles[p].node.id == nid) {
                        if (hop < poles[p].hop_distance) {
                            poles[p].hop_distance = hop;
                        }
                        seen = true;
                        break;
                    }
                }
                if (seen) {
                    continue;
                }
                if (pole_count >= pole_cap) {
                    int ncap = pole_cap * 2;
                    extracted_pole_t *npoles = (extracted_pole_t *)realloc(
                        poles, (size_t)ncap * sizeof(extracted_pole_t));
                    if (!npoles) {
                        cbm_store_traverse_free(&trav);
                        free_extracted_poles(poles, pole_count);
                        cbm_gov_rules_free(active_rules, active_rule_count);
                        free(active_rules_json);
                        cbm_project_free_fields(&proj);
                        return -1;
                    }
                    poles = npoles;
                    pole_cap = ncap;
                }
                memset(&poles[pole_count], 0, sizeof(extracted_pole_t));
                cbm_store_find_node_by_id(store, nid, &poles[pole_count].node);
                poles[pole_count].hop_distance = hop;
                cbm_harness_compute_node_effects(store, project, nid, 2, &poles[pole_count].effects);
                char *tmp_span = read_file_span(root_path, poles[pole_count].node.file_path,
                                                poles[pole_count].node.start_line,
                                                poles[pole_count].node.end_line);
                poles[pole_count].signature = synthesize_skeleton_signature(&poles[pole_count].node, tmp_span);
                free(tmp_span);
                poles[pole_count].code_span = NULL; /* Skeleton mode omits raw source body! */
                pole_count++;
            }
            cbm_store_traverse_free(&trav);
        }
    }

    /* 3. Compute coverage_revision and coverage_digest across candidate files (sorted deterministically) */
    const char **candidate_files = (const char **)calloc((size_t)(pole_count + 1), sizeof(char *));
    int cf_count = 0;
    if (candidate_files) {
        for (int i = 0; i < pole_count; i++) {
            const char *fp = poles[i].node.file_path;
            if (!fp || !fp[0]) {
                continue;
            }
            bool dup = false;
            for (int j = 0; j < cf_count; j++) {
                if (strcmp(candidate_files[j], fp) == 0) {
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                candidate_files[cf_count++] = fp;
            }
        }
        if (cf_count > 1) {
            qsort((void *)candidate_files, (size_t)cf_count, sizeof(char *), cmp_str_ptr);
        }
    }

    char coverage_rev[65] = {0};
    bool coverage_complete = true;
    char *coverage_digest_json = NULL;
    cbm_harness_compute_coverage_revision(store, project, candidate_files, cf_count,
                                          coverage_rev, &coverage_complete, &coverage_digest_json);
    free((void *)candidate_files);

    /* 4. Compute composite snapshot_id */
    char graph_gen[64] = {0};
    cbm_store_generation(store, graph_gen, sizeof(graph_gen));
    char snapshot_id[65] = {0};
    cbm_harness_compute_snapshot_id(graph_gen, rules_rev, coverage_rev, snapshot_id);
    if (out_snapshot_id) {
        memcpy(out_snapshot_id, snapshot_id, 65);
    }

    /* 5. Build Typed Seam Map across extracted poles (Task 07, I04-I06, F07) */
    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
    yyjson_mut_doc_set_root(mdoc, mroot);

    yyjson_mut_val *seams_arr = yyjson_mut_arr(mdoc);
    bool any_unresolved_seam = false;
    int seam_seq = 0;

    for (int i = 0; i < pole_count; i++) {
        cbm_edge_t *edges = NULL;
        int edge_count = 0;
        if (cbm_store_find_edges_by_source(store, poles[i].node.id, &edges, &edge_count) != CBM_STORE_OK) {
            continue;
        }
        for (int e = 0; e < edge_count; e++) {
            int target_idx = -1;
            for (int j = 0; j < pole_count; j++) {
                if (poles[j].node.id == edges[e].target_id) {
                    target_idx = j;
                    break;
                }
            }
            if (target_idx < 0 && edges[e].target_id != 0) {
                continue;
            }

            seam_seq++;
            char seam_id_buf[64];
            snprintf(seam_id_buf, sizeof(seam_id_buf), "SEAM-%03d", seam_seq);

            const char *seam_kind = "pure_call";
            const char *risk_level = "low";
            str_set_t seam_flags = {0};

            if (edges[e].properties_json &&
                (contains_token_ci(edges[e].properties_json, "\"unresolved\":true") ||
                 contains_token_ci(edges[e].properties_json, "\"dynamic\":true"))) {
                any_unresolved_seam = true;
                risk_level = "high";
                str_set_add(&seam_flags, "unresolved_call_edge");
            }

            if (target_idx >= 0) {
                const cbm_effect_fingerprint_t *tfp = &poles[target_idx].effects;
                if (tfp->writes_db != CBM_EFFECT_ABSENT) {
                    seam_kind = "db_transaction";
                    risk_level = "high";
                } else if (tfp->network_io != CBM_EFFECT_ABSENT ||
                           tfp->spawns_process != CBM_EFFECT_ABSENT) {
                    seam_kind = "ipc_rpc";
                    risk_level = "high";
                } else if (tfp->fs_mutation != CBM_EFFECT_ABSENT) {
                    seam_kind = "io_boundary";
                    risk_level = "medium";
                } else if (tfp->global_state_write != CBM_EFFECT_ABSENT) {
                    seam_kind = "shared_state";
                    risk_level = "medium";
                }
                if (cbm_effect_has_any_unknown(tfp)) {
                    any_unresolved_seam = true;
                    risk_level = "high";
                    str_set_add(&seam_flags, "callee_effect_unknown");
                }
            }

            yyjson_mut_val *sobj = yyjson_mut_obj(mdoc);
            yyjson_mut_obj_add_strcpy(mdoc, sobj, "seam_id", seam_id_buf);
            yyjson_mut_obj_add_strcpy(mdoc, sobj, "caller_pole",
                                      poles[i].node.qualified_name ? poles[i].node.qualified_name : "");
            yyjson_mut_obj_add_strcpy(mdoc, sobj, "callee_pole",
                                      (target_idx >= 0 && poles[target_idx].node.qualified_name)
                                          ? poles[target_idx].node.qualified_name
                                          : "UNRESOLVED");
            yyjson_mut_obj_add_strcpy(mdoc, sobj, "edge_type",
                                      edges[e].type ? edges[e].type : "CALLS");
            yyjson_mut_obj_add_strcpy(mdoc, sobj, "seam_kind", seam_kind);
            yyjson_mut_obj_add_strcpy(mdoc, sobj, "contract_shape",
                                      (target_idx >= 0 && poles[target_idx].signature)
                                          ? poles[target_idx].signature
                                          : "unknown(...)");
            yyjson_mut_obj_add_strcpy(mdoc, sobj, "risk_level", risk_level);

            char *flags_json = str_set_to_json(&seam_flags);
            yyjson_doc *fdoc = yyjson_read(flags_json, strlen(flags_json), 0);
            if (fdoc && yyjson_doc_get_root(fdoc)) {
                yyjson_mut_obj_add_val(mdoc, sobj, "uncertainty_flags",
                                       yyjson_val_mut_copy(mdoc, yyjson_doc_get_root(fdoc)));
            }
            if (fdoc) {
                yyjson_doc_free(fdoc);
            }
            free(flags_json);
            str_set_free(&seam_flags);

            yyjson_mut_arr_append(seams_arr, sobj);
        }
        cbm_store_free_edges(edges, edge_count);
    }

    /* 6. Build candidates array with asymmetric bifocal representation (U09, I01-I03) */
    yyjson_mut_val *cands_arr = yyjson_mut_arr(mdoc);
    bool any_effect_unknown = false;
    for (int i = 0; i < pole_count; i++) {
        if (cbm_effect_has_any_unknown(&poles[i].effects)) {
            any_effect_unknown = true;
        }
        yyjson_mut_val *cobj = yyjson_mut_obj(mdoc);
        char pole_id_buf[64];
        snprintf(pole_id_buf, sizeof(pole_id_buf), "POLE-%03d", i + 1);
        yyjson_mut_obj_add_strcpy(mdoc, cobj, "pole_id", pole_id_buf);
        yyjson_mut_obj_add_strcpy(mdoc, cobj, "qualified_name",
                                  poles[i].node.qualified_name ? poles[i].node.qualified_name : "");
        yyjson_mut_obj_add_strcpy(mdoc, cobj, "file_path",
                                  poles[i].node.file_path ? poles[i].node.file_path : "");
        int sline = poles[i].node.start_line >= 1 ? poles[i].node.start_line : 1;
        int eline = poles[i].node.end_line >= sline ? poles[i].node.end_line : sline;
        yyjson_mut_obj_add_int(mdoc, cobj, "start_line", sline);
        yyjson_mut_obj_add_int(mdoc, cobj, "end_line", eline);
        yyjson_mut_obj_add_int(mdoc, cobj, "hop_distance", poles[i].hop_distance);

        yyjson_mut_val *fobj = yyjson_mut_obj(mdoc);
        if (poles[i].hop_distance == 0) {
            yyjson_mut_obj_add_strcpy(mdoc, fobj, "mode", "integral");
            yyjson_mut_obj_add_strcpy(mdoc, fobj, "code",
                                      poles[i].code_span ? poles[i].code_span : "");
        } else {
            /* Skeleton mode: contract capsule ONLY, never full source body (`code`) */
            yyjson_mut_obj_add_strcpy(mdoc, fobj, "mode", "skeleton");
            yyjson_mut_val *skobj = yyjson_mut_obj(mdoc);
            yyjson_mut_obj_add_strcpy(mdoc, skobj, "signature",
                                      poles[i].signature ? poles[i].signature : "");
            yyjson_mut_obj_add_strcpy(mdoc, skobj, "label",
                                      poles[i].node.label ? poles[i].node.label : "Symbol");
            yyjson_mut_obj_add_val(mdoc, fobj, "skeleton", skobj);
        }
        yyjson_mut_obj_add_val(mdoc, cobj, "focus", fobj);
        add_effect_fingerprint_to_json(mdoc, cobj, &poles[i].effects);
        yyjson_mut_arr_append(cands_arr, cobj);
    }

    /* 7. Filter tradition_slice matching candidate files and check blocking conflicts */
    yyjson_mut_val *trad_arr = yyjson_mut_arr(mdoc);
    bool has_blocking_tradition_conflict = false;
    for (int r = 0; r < active_rule_count; r++) {
        bool matches = (pole_count == 0);
        for (int p = 0; p < pole_count; p++) {
            if (glob_matches_path(active_rules[r].scope_glob, poles[p].node.file_path)) {
                matches = true;
                break;
            }
        }
        if (!matches) {
            continue;
        }
        /* Check if rule is an EFFECT_RESTRICTION or LAYER_BOUNDARY that conflicts with pole effects */
        if (active_rules[r].payload_json &&
            contains_token_ci(active_rules[r].payload_json, "\"forbid_writes_db\":true")) {
            for (int p = 0; p < pole_count; p++) {
                if (poles[p].hop_distance == 0 &&
                    poles[p].effects.writes_db != CBM_EFFECT_ABSENT) {
                    has_blocking_tradition_conflict = true;
                }
            }
        }

        yyjson_mut_val *robj = yyjson_mut_obj(mdoc);
        yyjson_mut_obj_add_strcpy(mdoc, robj, "rule_id", active_rules[r].rule_id);
        yyjson_mut_obj_add_strcpy(mdoc, robj, "scope_glob", active_rules[r].scope_glob);
        yyjson_mut_obj_add_strcpy(mdoc, robj, "constraint_kind", active_rules[r].constraint_kind);
        yyjson_mut_obj_add_strcpy(mdoc, robj, "status", active_rules[r].status);
        yyjson_mut_obj_add_strcpy(mdoc, robj, "effective_status", active_rules[r].effective_status);
        yyjson_mut_obj_add_int(mdoc, robj, "review_due_at", active_rules[r].review_due_at);
        yyjson_doc *pdoc = yyjson_read(active_rules[r].payload_json,
                                       strlen(active_rules[r].payload_json), 0);
        if (pdoc && yyjson_doc_get_root(pdoc)) {
            yyjson_mut_obj_add_val(mdoc, robj, "payload",
                                   yyjson_val_mut_copy(mdoc, yyjson_doc_get_root(pdoc)));
        }
        if (pdoc) {
            yyjson_doc_free(pdoc);
        }
        yyjson_mut_arr_append(trad_arr, robj);
    }

    /* 8. Compute fail-closed Fast-Path eligibility for task_signature */
    const char *safe_change_kind = (change_kind && change_kind[0]) ? change_kind : "pure_refactor";
    bool kind_ok = is_fast_path_change_kind(safe_change_kind);
    bool fast_path_ok = (pole_count > 0) &&
                        !any_unresolved_seam &&
                        !any_effect_unknown &&
                        coverage_complete &&
                        !has_blocking_tradition_conflict &&
                        kind_ok;

    yyjson_mut_val *tsig = yyjson_mut_obj(mdoc);
    yyjson_mut_obj_add_strcpy(mdoc, tsig, "task_id", (task_id && task_id[0]) ? task_id : "TASK-DEFAULT");
    yyjson_mut_obj_add_strcpy(mdoc, tsig, "change_kind", safe_change_kind);
    yyjson_mut_obj_add_int(mdoc, tsig, "k_hop", k_hop);
    yyjson_mut_val *spoles_arr = yyjson_mut_arr(mdoc);
    for (int i = 0; i < seed_pole_count; i++) {
        if (seed_poles[i]) {
            yyjson_mut_arr_add_strcpy(mdoc, spoles_arr, seed_poles[i]);
        }
    }
    yyjson_mut_obj_add_val(mdoc, tsig, "poles", spoles_arr);
    yyjson_mut_obj_add_bool(mdoc, tsig, "fast_path_eligible", fast_path_ok);

    yyjson_mut_val *reasons_arr = yyjson_mut_arr(mdoc);
    if (fast_path_ok) {
        yyjson_mut_arr_add_strcpy(mdoc, reasons_arr, "all_seams_resolved");
        yyjson_mut_arr_add_strcpy(mdoc, reasons_arr, "zero_effect_uncertainty");
        yyjson_mut_arr_add_strcpy(mdoc, reasons_arr, "coverage_complete");
        yyjson_mut_arr_add_strcpy(mdoc, reasons_arr, "no_blocking_tradition_conflicts");
        yyjson_mut_arr_add_strcpy(mdoc, reasons_arr, "compatible_change_kind");
    } else {
        if (pole_count == 0)
            yyjson_mut_arr_add_strcpy(mdoc, reasons_arr, "no_poles_resolved");
        if (any_unresolved_seam)
            yyjson_mut_arr_add_strcpy(mdoc, reasons_arr, "unresolved_seam_present");
        if (any_effect_unknown)
            yyjson_mut_arr_add_strcpy(mdoc, reasons_arr, "effect_uncertainty_unknown");
        if (!coverage_complete)
            yyjson_mut_arr_add_strcpy(mdoc, reasons_arr, "coverage_gap_detected");
        if (has_blocking_tradition_conflict)
            yyjson_mut_arr_add_strcpy(mdoc, reasons_arr, "blocking_tradition_conflict");
        if (!kind_ok)
            yyjson_mut_arr_add_strcpy(mdoc, reasons_arr, "incompatible_change_kind");
    }
    yyjson_mut_obj_add_val(mdoc, tsig, "eligibility_reasons", reasons_arr);

    /* Assemble root bundle object (note: omit volatile evaluation_time before computing bundle_hash per 003 §2.3) */
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "schema_version", "2.0.0");
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "extractor_version", "2.0.0");
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "project", project);
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "snapshot_id", snapshot_id);
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "graph_generation", graph_gen);
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "rules_revision", rules_rev);
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "coverage_revision", coverage_rev);
    yyjson_mut_obj_add_val(mdoc, mroot, "task_signature", tsig);

    yyjson_doc *cdoc = yyjson_read(coverage_digest_json, strlen(coverage_digest_json), 0);
    if (cdoc && yyjson_doc_get_root(cdoc)) {
        yyjson_mut_obj_add_val(mdoc, mroot, "coverage_digest",
                               yyjson_val_mut_copy(mdoc, yyjson_doc_get_root(cdoc)));
    }
    if (cdoc) {
        yyjson_doc_free(cdoc);
    }

    yyjson_mut_obj_add_val(mdoc, mroot, "candidates", cands_arr);
    yyjson_mut_obj_add_val(mdoc, mroot, "seam_map", seams_arr);
    yyjson_mut_obj_add_val(mdoc, mroot, "tradition_slice", trad_arr);

    /* Compute canonical bundle_hash over payload before adding bundle_hash field */
    size_t pre_len = 0;
    char *pre_json = yyjson_mut_write(mdoc, 0, &pre_len);
    char bhash[65] = {0};
    if (pre_json) {
        cbm_canonical_json_sha256(pre_json, bhash);
        free(pre_json);
    }
    if (out_bundle_hash) {
        memcpy(out_bundle_hash, bhash, 65);
    }
    yyjson_mut_obj_add_strcpy(mdoc, mroot, "bundle_hash", bhash);

    size_t final_len = 0;
    char *final_raw = yyjson_mut_write(mdoc, 0, &final_len);
    yyjson_mut_doc_free(mdoc);

    char *canonical_bundle = NULL;
    if (final_raw) {
        cbm_canonical_json(final_raw, &canonical_bundle);
        free(final_raw);
    }

    free(coverage_digest_json);
    free(active_rules_json);
    cbm_gov_rules_free(active_rules, active_rule_count);
    free_extracted_poles(poles, pole_count);
    cbm_project_free_fields(&proj);

    if (!canonical_bundle) {
        return -1;
    }
    *out_bundle_json = canonical_bundle;
    return 0;
}

int cbm_harness_evaluate_fast_path(cbm_store_t *store,
                                   cbm_gov_store_t *gov_store,
                                   const char *project,
                                   const char *bundle_json,
                                   const char *change_kind_override,
                                   int64_t evaluation_time,
                                   cbm_fast_path_verdict_t *out_verdict,
                                   char **out_verdict_json) {
    if (!bundle_json || !out_verdict) {
        return -1;
    }
    memset(out_verdict, 0, sizeof(*out_verdict));
    if (out_verdict_json) {
        *out_verdict_json = NULL;
    }

    yyjson_doc *doc = yyjson_read(bundle_json, strlen(bundle_json), 0);
    if (!doc) {
        return -1;
    }
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return -1;
    }

    const char *snap = yyjson_get_str(yyjson_obj_get(root, "snapshot_id"));
    const char *ggen = yyjson_get_str(yyjson_obj_get(root, "graph_generation"));
    const char *rrev = yyjson_get_str(yyjson_obj_get(root, "rules_revision"));
    const char *crev = yyjson_get_str(yyjson_obj_get(root, "coverage_revision"));
    const char *bhash = yyjson_get_str(yyjson_obj_get(root, "bundle_hash"));

    out_verdict->snapshot_id = hv2_strdup(snap ? snap : "");
    out_verdict->graph_generation = hv2_strdup(ggen ? ggen : "");
    out_verdict->rules_revision = hv2_strdup(rrev ? rrev : "");
    out_verdict->coverage_revision = hv2_strdup(crev ? crev : "");
    out_verdict->bundle_hash = hv2_strdup(bhash ? bhash : "");

    str_set_t elig = {0};
    str_set_t disq = {0};

    /* 1. Check seams */
    bool seams_ok = true;
    yyjson_val *seams = yyjson_obj_get(root, "seam_map");
    if (seams && yyjson_is_arr(seams)) {
        yyjson_arr_iter it;
        yyjson_arr_iter_init(seams, &it);
        yyjson_val *s;
        while ((s = yyjson_arr_iter_next(&it)) != NULL) {
            const char *callee = yyjson_get_str(yyjson_obj_get(s, "callee_pole"));
            if (!callee || strcmp(callee, "UNRESOLVED") == 0) {
                seams_ok = false;
                str_set_add(&disq, "unresolved_seam_callee");
            }
            yyjson_val *uflags = yyjson_obj_get(s, "uncertainty_flags");
            if (uflags && yyjson_is_arr(uflags) && yyjson_arr_size(uflags) > 0) {
                seams_ok = false;
                str_set_add(&disq, "seam_uncertainty_flags_non_empty");
            }
        }
    }
    out_verdict->all_seams_resolved = seams_ok;
    if (seams_ok) {
        str_set_add(&elig, "all_seams_resolved");
    }

    /* 2. Check candidate effects for any "unknown" or invalid effect enum value */
    bool effects_ok = true;
    yyjson_val *cands = yyjson_obj_get(root, "candidates");
    if (!cands || !yyjson_is_arr(cands) || yyjson_arr_size(cands) == 0) {
        effects_ok = false;
        str_set_add(&disq, "empty_candidates");
    } else {
        yyjson_arr_iter it;
        yyjson_arr_iter_init(cands, &it);
        yyjson_val *c;
        while ((c = yyjson_arr_iter_next(&it)) != NULL) {
            yyjson_val *eff = yyjson_obj_get(c, "effects");
            if (!eff || !yyjson_is_obj(eff)) {
                effects_ok = false;
                str_set_add(&disq, "missing_effects_object");
                continue;
            }
            const char *fields[] = {
                "writes_db", "network_io", "fs_mutation",
                "spawns_process", "global_state_write",
                "throws_or_panics", "dynamic_dispatch"
            };
            for (size_t f = 0; f < 7; f++) {
                const char *v = yyjson_get_str(yyjson_obj_get(eff, fields[f]));
                if (!v || (strcmp(v, "absent") != 0 && strcmp(v, "present") != 0)) {
                    effects_ok = false;
                    str_set_add(&disq, "effect_uncertainty_unknown");
                }
            }
            yyjson_val *urs = yyjson_obj_get(eff, "uncertainty_reasons");
            if (urs && yyjson_is_arr(urs) && yyjson_arr_size(urs) > 0) {
                effects_ok = false;
                str_set_add(&disq, "effect_uncertainty_reasons_present");
            }
        }
    }
    out_verdict->zero_uncertainty_effects = effects_ok;
    if (effects_ok) {
        str_set_add(&elig, "zero_uncertainty_effects");
    }

    /* 3. Check coverage_digest and verify against live store coverage if store is present */
    bool cov_ok = true;
    yyjson_val *cdig = yyjson_obj_get(root, "coverage_digest");
    if (!cdig || !yyjson_is_obj(cdig)) {
        cov_ok = false;
        str_set_add(&disq, "missing_coverage_digest");
    } else {
        yyjson_val *cc = yyjson_obj_get(cdig, "coverage_complete");
        if (!cc || !yyjson_get_bool(cc)) {
            cov_ok = false;
            str_set_add(&disq, "coverage_incomplete");
        }
        yyjson_val *gaps = yyjson_obj_get(cdig, "gaps");
        if (gaps && yyjson_is_arr(gaps) && yyjson_arr_size(gaps) > 0) {
            cov_ok = false;
            str_set_add(&disq, "coverage_gaps_present");
        }
    }
    if (store && project && project[0] && cands && yyjson_is_arr(cands)) {
        size_t ccount = yyjson_arr_size(cands);
        const char **cfiles = (const char **)calloc(ccount + 1, sizeof(char *));
        int cf_cnt = 0;
        if (cfiles) {
            yyjson_arr_iter it;
            yyjson_arr_iter_init(cands, &it);
            yyjson_val *c;
            while ((c = yyjson_arr_iter_next(&it)) != NULL) {
                const char *fp = yyjson_get_str(yyjson_obj_get(c, "file_path"));
                if (!fp || !fp[0]) continue;
                bool dup = false;
                for (int k = 0; k < cf_cnt; k++) {
                    if (strcmp(cfiles[k], fp) == 0) { dup = true; break; }
                }
                if (!dup) cfiles[cf_cnt++] = fp;
            }
            if (cf_cnt > 1) {
                qsort((void *)cfiles, (size_t)cf_cnt, sizeof(char *), cmp_str_ptr);
            }
            char live_crev[65] = {0};
            bool live_ccomp = false;
            if (cbm_harness_compute_coverage_revision(store, project, cfiles, cf_cnt,
                                                      live_crev, &live_ccomp, NULL) == 0) {
                if (!live_ccomp) {
                    cov_ok = false;
                    str_set_add(&disq, "live_coverage_incomplete");
                }
                if (!out_verdict->coverage_revision || !out_verdict->coverage_revision[0]) {
                    free(out_verdict->coverage_revision);
                    out_verdict->coverage_revision = hv2_strdup(live_crev);
                } else if (strcmp(out_verdict->coverage_revision, live_crev) != 0) {
                    cov_ok = false;
                    str_set_add(&disq, "stale_coverage_revision");
                }
            }
            free((void *)cfiles);
        }
    }
    out_verdict->coverage_complete = cov_ok;
    if (cov_ok) {
        str_set_add(&elig, "coverage_complete");
    }

    /* 4. Check tradition conflicts and snapshot freshness against live stores if provided */
    bool trad_ok = true;
    yyjson_val *bv = yyjson_obj_get(root, "blocking_violations");
    if (bv && yyjson_is_arr(bv) && yyjson_arr_size(bv) > 0) {
        trad_ok = false;
        str_set_add(&disq, "blocking_violations_present");
    }
    if (gov_store && evaluation_time >= 0) {
        char live_rules_rev[65] = {0};
        if (cbm_gov_evaluate_active_rules(gov_store, evaluation_time, NULL, NULL, live_rules_rev, NULL) != CBM_GOV_OK) {
            trad_ok = false;
            str_set_add(&disq, "clock_or_governance_uncertain");
        } else if (rrev && rrev[0] && strcmp(rrev, live_rules_rev) != 0) {
            trad_ok = false;
            str_set_add(&disq, "stale_rules_revision");
        }
    }
    if (store && ggen && ggen[0]) {
        char live_ggen[64] = {0};
        cbm_store_generation(store, live_ggen, sizeof(live_ggen));
        if (strcmp(ggen, live_ggen) != 0) {
            trad_ok = false;
            str_set_add(&disq, "stale_graph_generation");
        }
    }
    out_verdict->no_blocking_tradition_conflicts = trad_ok;
    if (trad_ok) {
        str_set_add(&elig, "no_blocking_tradition_conflicts");
    }

    /* 5. Check change_kind compatibility */
    const char *ckind = change_kind_override;
    yyjson_val *tsig = yyjson_obj_get(root, "task_signature");
    if ((!ckind || !ckind[0]) && tsig && yyjson_is_obj(tsig)) {
        ckind = yyjson_get_str(yyjson_obj_get(tsig, "change_kind"));
    }
    bool kind_ok = is_fast_path_change_kind(ckind);
    out_verdict->change_kind_compatible = kind_ok;
    if (kind_ok) {
        str_set_add(&elig, "change_kind_compatible");
    } else {
        str_set_add(&disq, "incompatible_change_kind");
    }

    out_verdict->eligible = seams_ok && effects_ok && cov_ok && trad_ok && kind_ok;
    out_verdict->eligibility_reasons_json = str_set_to_json(&elig);
    out_verdict->disqualifying_reasons_json = str_set_to_json(&disq);

    str_set_free(&elig);
    str_set_free(&disq);
    yyjson_doc_free(doc);

    if (out_verdict_json) {
        yyjson_mut_doc *mdoc = yyjson_mut_doc_new(NULL);
        yyjson_mut_val *mroot = yyjson_mut_obj(mdoc);
        yyjson_mut_doc_set_root(mdoc, mroot);

        yyjson_mut_obj_add_bool(mdoc, mroot, "eligible", out_verdict->eligible);
        yyjson_mut_obj_add_bool(mdoc, mroot, "all_seams_resolved", out_verdict->all_seams_resolved);
        yyjson_mut_obj_add_bool(mdoc, mroot, "zero_uncertainty_effects", out_verdict->zero_uncertainty_effects);
        yyjson_mut_obj_add_bool(mdoc, mroot, "coverage_complete", out_verdict->coverage_complete);
        yyjson_mut_obj_add_bool(mdoc, mroot, "no_blocking_tradition_conflicts", out_verdict->no_blocking_tradition_conflicts);
        yyjson_mut_obj_add_bool(mdoc, mroot, "change_kind_compatible", out_verdict->change_kind_compatible);
        yyjson_mut_obj_add_strcpy(mdoc, mroot, "snapshot_id", out_verdict->snapshot_id);
        yyjson_mut_obj_add_strcpy(mdoc, mroot, "bundle_hash", out_verdict->bundle_hash);

        yyjson_doc *edoc = yyjson_read(out_verdict->eligibility_reasons_json,
                                       strlen(out_verdict->eligibility_reasons_json), 0);
        if (edoc && yyjson_doc_get_root(edoc)) {
            yyjson_mut_obj_add_val(mdoc, mroot, "eligibility_reasons",
                                   yyjson_val_mut_copy(mdoc, yyjson_doc_get_root(edoc)));
        }
        if (edoc) {
            yyjson_doc_free(edoc);
        }

        yyjson_doc *ddoc = yyjson_read(out_verdict->disqualifying_reasons_json,
                                       strlen(out_verdict->disqualifying_reasons_json), 0);
        if (ddoc && yyjson_doc_get_root(ddoc)) {
            yyjson_mut_obj_add_val(mdoc, mroot, "disqualifying_reasons",
                                   yyjson_val_mut_copy(mdoc, yyjson_doc_get_root(ddoc)));
        }
        if (ddoc) {
            yyjson_doc_free(ddoc);
        }

        size_t vlen = 0;
        char *vraw = yyjson_mut_write(mdoc, 0, &vlen);
        yyjson_mut_doc_free(mdoc);
        if (vraw) {
            cbm_canonical_json(vraw, out_verdict_json);
            free(vraw);
        }
    }

    return 0;
}

void cbm_fast_path_verdict_free(cbm_fast_path_verdict_t *v) {
    if (!v) {
        return;
    }
    free(v->eligibility_reasons_json);
    free(v->disqualifying_reasons_json);
    free(v->snapshot_id);
    free(v->graph_generation);
    free(v->rules_revision);
    free(v->coverage_revision);
    free(v->bundle_hash);
    memset(v, 0, sizeof(*v));
}
