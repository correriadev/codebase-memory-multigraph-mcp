/*
 * pass_theme_relations.c - Materialize explicit Markdown topic links.
 *
 * A Section may declare `CBM_RELATIONS_V1[TYPE=T-02; TYPE=T-03]` in its
 * indexed docstring. Targets resolve only to T-xx Sections in the same file,
 * so a declaration cannot accidentally bind to an unrelated project's topic.
 */
#include "pipeline/pipeline_internal.h"

#include "foundation/log.h"
#include "graph_buffer/graph_buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { THEME_RELATION_ENTRY_CAP = 96, THEME_RELATION_TYPE_CAP = 48,
       THEME_RELATION_REF_CAP = 16 };

static const char *const k_relation_types[] = {
    "APPLIES_TO", "CONSTRAINS", "MOTION_SPECIFIED_BY", "SPECIALIZES", "SUPPORTED_BY",
    "SUPPORTS",  "USES",       "VERIFIED_WITH",
};

static bool relation_type_allowed(const char *type) {
    for (size_t i = 0; i < sizeof(k_relation_types) / sizeof(k_relation_types[0]); i++) {
        if (strcmp(type, k_relation_types[i]) == 0) {
            return true;
        }
    }
    return false;
}

static char *trim_ascii(char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
        text++;
    }
    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t' || text[len - 1] == '\r' ||
                       text[len - 1] == '\n')) {
        text[--len] = '\0';
    }
    return text;
}

static bool stable_topic_ref(const char *ref) {
    return ref && strlen(ref) == 4 && ref[0] == 'T' && ref[1] == '-' && ref[2] >= '0' &&
           ref[2] <= '9' && ref[3] >= '0' && ref[3] <= '9';
}

static const cbm_gbuf_node_t *find_topic(const cbm_gbuf_node_t *const *sections, int count,
                                         const cbm_gbuf_node_t *source, const char *ref) {
    if (!source->file_path || !source->name) {
        return NULL;
    }
    const cbm_gbuf_node_t *match = NULL;
    size_t ref_len = strlen(ref);
    for (int i = 0; i < count; i++) {
        const cbm_gbuf_node_t *candidate = sections[i];
        if (!candidate || candidate->id == source->id || !candidate->file_path ||
            !candidate->name || strcmp(candidate->file_path, source->file_path) != 0 ||
            strncmp(candidate->name, ref, ref_len) != 0 ||
            (candidate->name[ref_len] != '\0' && candidate->name[ref_len] != ' ' &&
             candidate->name[ref_len] != ':' && candidate->name[ref_len] != '.')) {
            continue;
        }
        if (match) {
            return NULL; /* Duplicate topic IDs are ambiguous, never guess. */
        }
        match = candidate;
    }
    return match;
}

typedef struct {
    cbm_pipeline_ctx_t *ctx;
    const cbm_gbuf_node_t *const *sections;
    int section_count;
    int edges_written;
    int invalid_entries;
    bool failed;
} relation_pass_state_t;

static void process_relation_entry(relation_pass_state_t *state, const cbm_gbuf_node_t *source,
                                   char *entry) {
    char *pair = trim_ascii(entry);
    if (!pair[0]) {
        return;
    }
    char *equals = strchr(pair, '=');
    if (!equals || equals == pair || equals[1] == '\0' || strchr(equals + 1, '=')) {
        state->invalid_entries++;
        cbm_log_warn("theme_relation.invalid", "source", source->qualified_name, "entry", pair);
        return;
    }
    *equals = '\0';
    char *type = trim_ascii(pair);
    char *ref = trim_ascii(equals + 1);
    if (strlen(type) >= THEME_RELATION_TYPE_CAP || !relation_type_allowed(type) ||
        !stable_topic_ref(ref)) {
        state->invalid_entries++;
        cbm_log_warn("theme_relation.invalid", "source", source->qualified_name, "entry", pair);
        return;
    }

    const cbm_gbuf_node_t *target =
        find_topic(state->sections, state->section_count, source, ref);
    if (!target) {
        state->invalid_entries++;
        cbm_log_warn("theme_relation.unresolved", "source", source->qualified_name, "target", ref);
        return;
    }

    char properties[96];
    snprintf(properties, sizeof(properties),
             "{\"source\":\"CBM_RELATIONS_V1\",\"target_ref\":\"%s\"}", ref);
    if (cbm_gbuf_insert_edge(state->ctx->gbuf, source->id, target->id, type, properties) == 0) {
        state->failed = true;
        return;
    }
    state->edges_written++;
}

static void process_section(const cbm_gbuf_node_t *node, void *userdata) {
    relation_pass_state_t *state = (relation_pass_state_t *)userdata;
    if (state->failed || !node || !node->label || strcmp(node->label, "Section") != 0 ||
        !node->properties_json) {
        return;
    }

    /* The marker is in the Section's docstring, which is stored verbatim apart
     * from JSON escaping. The syntax intentionally uses no quotes or escapes. */
    const char *marker = strstr(node->properties_json, "CBM_RELATIONS_V1[");
    if (!marker) {
        return;
    }
    marker += strlen("CBM_RELATIONS_V1[");
    const char *end = strchr(marker, ']');
    if (!end) {
        state->invalid_entries++;
        cbm_log_warn("theme_relation.unterminated", "source", node->qualified_name);
        return;
    }

    while (marker < end) {
        const char *separator = memchr(marker, ';', (size_t)(end - marker));
        const char *entry_end = separator ? separator : end;
        size_t entry_len = (size_t)(entry_end - marker);
        if (entry_len >= THEME_RELATION_ENTRY_CAP) {
            state->invalid_entries++;
            cbm_log_warn("theme_relation.entry_too_long", "source", node->qualified_name);
        } else {
            char entry[THEME_RELATION_ENTRY_CAP];
            memcpy(entry, marker, entry_len);
            entry[entry_len] = '\0';
            process_relation_entry(state, node, entry);
        }
        if (!separator) {
            break;
        }
        marker = separator + 1;
    }
}

int cbm_pipeline_pass_theme_relations(cbm_pipeline_ctx_t *ctx) {
    if (!ctx || !ctx->gbuf) {
        return CBM_NOT_FOUND;
    }

    const cbm_gbuf_node_t **sections = NULL;
    int section_count = 0;
    if (cbm_gbuf_find_by_label(ctx->gbuf, "Section", &sections, &section_count) != 0) {
        return CBM_NOT_FOUND;
    }

    relation_pass_state_t state = {
        .ctx = ctx,
        .sections = sections,
        .section_count = section_count,
    };
    cbm_gbuf_foreach_node(ctx->gbuf, process_section, &state);
    if (state.failed) {
        return CBM_NOT_FOUND;
    }
    if (state.edges_written > 0 || state.invalid_entries > 0) {
        char edge_count[24];
        char invalid_count[24];
        snprintf(edge_count, sizeof(edge_count), "%d", state.edges_written);
        snprintf(invalid_count, sizeof(invalid_count), "%d", state.invalid_entries);
        cbm_log_info("theme_relation.done", "edges", edge_count, "invalid", invalid_count);
    }
    return 0;
}
