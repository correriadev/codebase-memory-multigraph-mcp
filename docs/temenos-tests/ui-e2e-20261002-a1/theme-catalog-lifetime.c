/* Focused ASan reproducer for theme_search request-string ownership.
 * Compile union_handler.c with function/data sections and --gc-sections so
 * only the actual catalog handlers are linked into this small test process. */
#include "../../../src/mcp/union_handler.h"
#include "../../../src/mcp/mcp.h"
#include "../../../src/foundation/compat_fs.h"
#include "../../../src/foundation/platform.h"
#include <yyjson/yyjson.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static CbmThemeRegistry test_registry;

int cbm_mcp_get_int_arg(const char *args_json, const char *key, int default_val) {
    (void)args_json;
    (void)key;
    return default_val;
}

CbmThemeRegistry *cbm_mcp_server_theme_registry(cbm_mcp_server_t *srv) {
    return srv ? &test_registry : NULL;
}

char *cbm_mcp_text_result(const char *text, bool is_error) {
    (void)is_error;
    if (!text) return NULL;
    size_t length = strlen(text) + 32;
    char *wrapped = (char *)malloc(length);
    if (!wrapped) return NULL;
    snprintf(wrapped, length, "{\"structuredContent\":%s}", text);
    return wrapped;
}

/* The backing topic nodes exist in the separately indexed real CBM project.
 * This focused unit harness supplies the same row shape so it can isolate
 * citation construction and fail-closed provenance handling under ASan. */
char *handle_search_graph(cbm_mcp_server_t *srv, const char *args) {
    (void)srv;
    (void)args;
    return strdup(
        "{\"structuredContent\":{\"cols\":[\"qn\",\"label\",\"file\",\"lines\",\"rank\"],"
        "\"rows\":[[\"HarnessKit-UI-Design-Practices.README.T-02-Spacing,-alignment,-and-layout-rhythm\","
        "\"Section\",\"README.md\",\"27-28\",-1.0]],\"total\":1}}"
    );
}

char *handle_query_graph(cbm_mcp_server_t *srv, const char *args) {
    (void)srv;
    (void)args;
    return strdup("{\"structuredContent\":{\"cols\":[\"source\",\"relationship\",\"target\"],\"rows\":[],\"total\":0}}");
}

void cbm_session_registry_init(CbmSessionRegistry *registry) { memset(registry, 0, sizeof(*registry)); }
void cbm_contract_registry_init(CbmContractRegistry *registry) { memset(registry, 0, sizeof(*registry)); }
void cbm_gateway_init(CbmGateway *gateway) { memset(gateway, 0, sizeof(*gateway)); }
void cbm_binding_ledger_init(CbmBindingLedger *ledger) { memset(ledger, 0, sizeof(*ledger)); }
void cbm_contest_registry_init(CbmContestRegistry *registry) { memset(registry, 0, sizeof(*registry)); }
const char *cbm_resolve_cache_dir(void) { return NULL; }
int cbm_horizon_pool_init(HorizonConnectionPool *pool, const char *storage_dir) {
    (void)storage_dir;
    memset(pool, 0, sizeof(*pool));
    return 0;
}

void cbm_refusal_emit(CbmRefusalCode code, const char *horizon_id, const char *reason) {
    (void)code;
    (void)horizon_id;
    (void)reason;
}

const char *cbm_refusal_code_string(CbmRefusalCode code) {
    if (code == CBM_REFUSAL_ANCHOR_NOT_FOUND) return "ANCHOR_NOT_FOUND";
    return "TEST_REFUSAL";
}

bool cbm_file_exists(const char *path) {
    return path && access(path, F_OK) == 0;
}

int cbm_rename_replace(const char *src, const char *dst) {
    (void)src;
    (void)dst;
    return 0;
}

int cbm_unlink(const char *path) {
    (void)path;
    return 0;
}

static yyjson_val *structured(char *response, yyjson_doc **owner) {
    *owner = yyjson_read(response, strlen(response), 0);
    yyjson_val *root = *owner ? yyjson_doc_get_root(*owner) : NULL;
    yyjson_val *structured_content = root ? yyjson_obj_get(root, "structuredContent") : NULL;
    return structured_content ? structured_content : root;
}

int main(void) {
    const char *registry_path = getenv("CBM_TEST_THEME_REGISTRY");
    const char *query;
    const char *search_args;
    if (registry_path && registry_path[0]) {
        if (!cbm_theme_registry_open(&test_registry, registry_path)) return 5;
        query = "HarnessKit";
        search_args = "{\"query\":\"HarnessKit\","
                      "\"namespace\":\"product-design/interface\"}";
    } else {
        cbm_theme_registry_init(&test_registry);
        CbmThemeEntry *entry = &test_registry.entries[0];
        snprintf(entry->theme_id, sizeof(entry->theme_id), "%s", "@harnesskit/ui-design-practices");
        snprintf(entry->name, sizeof(entry->name), "%s", "HarnessKit UI Design Practices");
        snprintf(entry->namespace, sizeof(entry->namespace), "%s", "product-design/interface");
        snprintf(entry->target_uri, sizeof(entry->target_uri), "%s", "cbm-project://HarnessKit-UI-Design-Practices");
        snprintf(entry->curator, sizeof(entry->curator), "%s", "codex-e2e");
        snprintf(entry->version, sizeof(entry->version), "%s", "1.0.0");
        snprintf(entry->description, sizeof(entry->description), "%s", "UI design practices");
        entry->status = CBM_THEME_ACTIVE;
        test_registry.count = 1;
        query = "HarnessKit";
        search_args = "{\"query\":\"HarnessKit\","
                      "\"namespace\":\"product-design/interface\"}";
    }
    cbm_mcp_server_t *server = (cbm_mcp_server_t *)(uintptr_t)1;
    char *response = handle_theme_search(server, search_args);
    if (!response) return 3;
    yyjson_doc *doc = NULL;
    yyjson_val *data = structured(response, &doc);
    yyjson_val *query_value = data ? yyjson_obj_get(data, "query") : NULL;
    yyjson_val *results = data ? yyjson_obj_get(data, "results") : NULL;
    bool ok = query_value && yyjson_is_str(query_value) &&
              strcmp(yyjson_get_str(query_value), query) == 0 &&
              results && yyjson_is_arr(results) && yyjson_arr_size(results) >= 1;
    if (!ok) {
        fprintf(stderr, "unexpected catalog response: %s\n", response);
    }
    if (doc) yyjson_doc_free(doc);
    free(response);
    if (!ok) return 4;

    puts("PASS: theme_search preserves the query and returns the reopened catalog hit");

    const char *citation_args =
        "{\"theme_id\":\"@harnesskit/ui-design-practices\",\"version\":\"1.0.0\","
        "\"query\":\"spacing\",\"limit\":30}";
    response = handle_theme_graph_search(server, citation_args);
    if (!response) return 6;
    doc = NULL;
    data = structured(response, &doc);
    yyjson_val *candidates = data ? yyjson_obj_get(data, "citation_candidates") : NULL;
    const char *expected_uri =
        "cbm://HarnessKit-UI-Design-Practices/README.md#"
        "HarnessKit-UI-Design-Practices.README.T-02-Spacing,-alignment,-and-layout-rhythm";
    bool citation_found = false;
    if (candidates && yyjson_is_arr(candidates)) {
        for (size_t i = 0; i < yyjson_arr_size(candidates); i++) {
            yyjson_val *candidate = yyjson_arr_get(candidates, i);
            yyjson_val *uri = candidate ? yyjson_obj_get(candidate, "node_uri") : NULL;
            if (uri && yyjson_is_str(uri) && strcmp(yyjson_get_str(uri), expected_uri) == 0) {
                citation_found = true;
            }
        }
    }
    if (doc) yyjson_doc_free(doc);
    free(response);
    if (!citation_found) {
        fprintf(stderr, "theme_graph_search did not return the exact section URI\n");
        return 7;
    }

    char valid_args[1024];
    snprintf(valid_args, sizeof(valid_args),
             "{\"theme_id\":\"@harnesskit/ui-design-practices\","
             "\"pinned_version\":\"1.0.0\",\"node_uri\":\"%s\"}", expected_uri);
    response = handle_validate_provenance(server, valid_args);
    if (!response) return 8;
    doc = NULL;
    data = structured(response, &doc);
    yyjson_val *valid = data ? yyjson_obj_get(data, "valid") : NULL;
    yyjson_val *anchor_verified = data ? yyjson_obj_get(data, "anchor_verified") : NULL;
    bool accepted_exact = valid && yyjson_is_true(valid) && anchor_verified && yyjson_is_true(anchor_verified);
    if (doc) yyjson_doc_free(doc);
    free(response);
    if (!accepted_exact) {
        fprintf(stderr, "validate_provenance rejected the exact indexed theme section\n");
        return 9;
    }

    char invalid_args[1024];
    snprintf(invalid_args, sizeof(invalid_args),
             "{\"theme_id\":\"@harnesskit/ui-design-practices\","
             "\"pinned_version\":\"1.0.0\",\"node_uri\":\"%s-typo\"}", expected_uri);
    response = handle_validate_provenance(server, invalid_args);
    if (!response) return 10;
    doc = NULL;
    data = structured(response, &doc);
    yyjson_val *is_error = data ? yyjson_obj_get(data, "isError") : NULL;
    yyjson_val *code = data ? yyjson_obj_get(data, "code") : NULL;
    bool rejected_bad_anchor = is_error && yyjson_is_true(is_error) &&
                               code && yyjson_is_str(code) &&
                               strcmp(yyjson_get_str(code), "ANCHOR_NOT_FOUND") == 0;
    if (doc) yyjson_doc_free(doc);
    free(response);
    if (!rejected_bad_anchor) {
        fprintf(stderr, "validate_provenance failed to reject a drifted theme anchor\n");
        return 11;
    }

    puts("PASS: thematic search emits the exact section URI; provenance accepts that anchor and rejects a drifted URI");
    return 0;
}
