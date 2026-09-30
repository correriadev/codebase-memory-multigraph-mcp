#include "mutation_gate.h"
#include "mutation_journal.h"

#include "../foundation/compat_fs.h"

#include <stdio.h>
#include <string.h>

static bool has_terminator(const char *value, size_t capacity) {
    return value && memchr(value, '\0', capacity) != NULL;
}

static bool is_nonempty_string(const char *value, size_t capacity) {
    return has_terminator(value, capacity) && value[0] != '\0';
}

static bool mutation_path_is_absolute(const char *path);

CbmMutationEffect cbm_mutation_classify(CbmMutationOperation operation,
                                        CbmMutationScope scope) {
    if (operation == CBM_MUTATION_OPERATION_READ) {
        return CBM_MUTATION_EFFECT_READ;
    }

    bool is_write = operation == CBM_MUTATION_OPERATION_CREATE ||
                    operation == CBM_MUTATION_OPERATION_MODIFY ||
                    operation == CBM_MUTATION_OPERATION_DELETE ||
                    operation == CBM_MUTATION_OPERATION_RENAME;
    if (!is_write || scope == CBM_MUTATION_SCOPE_UNKNOWN) {
        return CBM_MUTATION_EFFECT_AMBIGUOUS;
    }
    if (scope == CBM_MUTATION_SCOPE_REPOSITORY) {
        return CBM_MUTATION_EFFECT_REPOSITORY_WRITE;
    }
    if (scope == CBM_MUTATION_SCOPE_OUTSIDE) {
        return CBM_MUTATION_EFFECT_OUTSIDE_REPOSITORY;
    }
    return CBM_MUTATION_EFFECT_AMBIGUOUS;
}

bool cbm_host_work_context_is_valid(const CbmHostWorkContext *context) {
    if (!context ||
        (context->host != CBM_MUTATION_HOST_ANTIGRAVITY &&
         context->host != CBM_MUTATION_HOST_CODEX)) {
        return false;
    }
    return is_nonempty_string(context->context_id, sizeof(context->context_id));
}

bool cbm_change_grounding_is_valid(const CbmChangeGrounding *grounding) {
    if (!grounding ||
        !is_nonempty_string(grounding->intent_key, sizeof(grounding->intent_key))) {
        return false;
    }
    if (grounding->target_count == 0U ||
        grounding->target_count > CBM_MUTATION_SCOPE_MAX_TARGETS) {
        return false;
    }

    for (size_t index = 0; index < grounding->target_count; index++) {
        const CbmMutationIntentTarget *target = &grounding->targets[index];
        if (!is_nonempty_string(target->path, sizeof(target->path)) ||
            !mutation_path_is_absolute(target->path) ||
            (target->operation != CBM_MUTATION_OPERATION_CREATE &&
             target->operation != CBM_MUTATION_OPERATION_MODIFY &&
             target->operation != CBM_MUTATION_OPERATION_DELETE &&
             target->operation != CBM_MUTATION_OPERATION_RENAME)) {
            return false;
        }
        for (size_t previous = 0; previous < index; previous++) {
            if (grounding->targets[previous].operation == target->operation &&
                strcmp(grounding->targets[previous].path, target->path) == 0) {
                return false;
            }
        }
    }

    bool has_reference = is_nonempty_string(grounding->reference, sizeof(grounding->reference));
    bool has_rationale = is_nonempty_string(grounding->rationale, sizeof(grounding->rationale));
    if (grounding->kind == CBM_GROUNDING_CANON_CITATION) {
        return has_reference && !has_rationale;
    }
    if (grounding->kind == CBM_GROUNDING_DECLARED_INVENTION) {
        return !has_reference && has_rationale;
    }
    return false;
}

static bool mutation_path_is_absolute(const char *path) {
    if (!path || !path[0]) return false;
#ifdef _WIN32
    if (path[0] == '\\' && path[1] == '\\') return true;
    return path[0] && path[1] == ':' && (path[2] == '/' || path[2] == '\\');
#else
    return path[0] == '/';
#endif
}

bool cbm_mutation_canonicalize_path(const char *path, char *canonical,
                                   size_t canonical_size) {
    if (!path || !canonical || canonical_size == 0U ||
        !mutation_path_is_absolute(path)) {
        return false;
    }
    char resolved[CBM_MUTATION_CANONICAL_PATH_MAX];
    if (cbm_canonical_path(path, resolved, sizeof(resolved))) {
        for (char *cursor = resolved; *cursor; cursor++) {
            if (*cursor == '\\') *cursor = '/';
        }
        size_t resolved_size = strlen(resolved) + 1U;
        if (resolved_size > canonical_size) return false;
        memcpy(canonical, resolved, resolved_size);
        return mutation_path_is_absolute(canonical);
    }

    char parent[CBM_MUTATION_CANONICAL_PATH_MAX];
    size_t path_len = strlen(path);
    if (path_len == 0U || path_len >= sizeof(parent)) return false;
    memcpy(parent, path, path_len + 1U);
    char *separator = strrchr(parent, '/');
    char *backslash = strrchr(parent, '\\');
    if (!separator || (backslash && backslash > separator)) separator = backslash;
    if (!separator || !separator[1] || strcmp(separator + 1, ".") == 0 ||
        strcmp(separator + 1, "..") == 0) {
        return false;
    }
    char leaf[CBM_MUTATION_CANONICAL_PATH_MAX];
    if (strlen(separator + 1) >= sizeof(leaf)) return false;
    snprintf(leaf, sizeof(leaf), "%s", separator + 1);
    if (separator == parent) {
        separator[1] = '\0';
    } else if (separator == parent + 2 && parent[1] == ':') {
        separator[1] = '\0';
    } else {
        *separator = '\0';
    }

    char canonical_parent[CBM_MUTATION_CANONICAL_PATH_MAX];
    if (!cbm_canonical_path(parent, canonical_parent, sizeof(canonical_parent))) return false;
    size_t parent_len = strlen(canonical_parent);
    int written = snprintf(resolved, sizeof(resolved), "%s%s%s", canonical_parent,
                           parent_len > 0U &&
                                   (canonical_parent[parent_len - 1U] == '/' ||
                                    canonical_parent[parent_len - 1U] == '\\')
                               ? "" : "/",
                           leaf);
    if (written <= 0 || (size_t)written >= sizeof(resolved)) return false;
    for (char *cursor = resolved; *cursor; cursor++) {
        if (*cursor == '\\') *cursor = '/';
    }
    if ((size_t)written + 1U > canonical_size) return false;
    memcpy(canonical, resolved, (size_t)written + 1U);
    return mutation_path_is_absolute(canonical);
}

static CbmMutationDecision mutation_refusal(CbmMutationRefusalCode code, const char *reason) {
    CbmMutationDecision decision = {0};
    decision.refusal.code = code;
    if (reason) snprintf(decision.refusal.reason, sizeof(decision.refusal.reason), "%s", reason);
    return decision;
}

static bool mutation_intent_covers_attempt(const CbmChangeGrounding *grounding,
                                           const CbmMutationAttempt *attempt) {
    if (!grounding || !attempt ||
        !is_nonempty_string(attempt->target_path, sizeof(attempt->target_path))) {
        return false;
    }
    bool source_covered = false;
    bool destination_covered = attempt->operation != CBM_MUTATION_OPERATION_RENAME;
    if (attempt->operation == CBM_MUTATION_OPERATION_RENAME &&
        (!is_nonempty_string(attempt->secondary_target_path,
                             sizeof(attempt->secondary_target_path)) ||
         strcmp(attempt->target_path, attempt->secondary_target_path) == 0)) {
        return false;
    }
    if (attempt->operation != CBM_MUTATION_OPERATION_RENAME &&
        attempt->secondary_target_path[0] != '\0') {
        return false;
    }
    for (size_t index = 0; index < grounding->target_count; index++) {
        if (grounding->targets[index].operation != attempt->operation) continue;
        if (strcmp(grounding->targets[index].path, attempt->target_path) == 0) {
            source_covered = true;
        }
        if (attempt->operation == CBM_MUTATION_OPERATION_RENAME &&
            strcmp(grounding->targets[index].path, attempt->secondary_target_path) == 0) {
            destination_covered = true;
        }
    }
    return source_covered && destination_covered;
}

CbmMutationDecision cbm_mutation_authorize_repository_write(
    struct CbmMutationJournal *journal, const CbmMutationAttempt *attempt) {
    if (!attempt) {
        return mutation_refusal(CBM_MUTATION_REFUSAL_EFFECT_UNKNOWN,
                                "Mutation attempt is missing.");
    }
    CbmMutationEffect effect = cbm_mutation_classify(attempt->operation, attempt->scope);
    if (effect == CBM_MUTATION_EFFECT_READ || effect == CBM_MUTATION_EFFECT_OUTSIDE_REPOSITORY) {
        CbmMutationDecision decision = {0};
        decision.permitted = true;
        return decision;
    }
    if (effect != CBM_MUTATION_EFFECT_REPOSITORY_WRITE &&
        effect != CBM_MUTATION_EFFECT_AMBIGUOUS) {
        return mutation_refusal(CBM_MUTATION_REFUSAL_EFFECT_UNKNOWN,
                                "The tool effect could not be classified safely.");
    }
    if (!cbm_host_work_context_is_valid(&attempt->host_context)) {
        return mutation_refusal(CBM_MUTATION_REFUSAL_SESSION_UNBOUND,
                                "The host did not provide a stable work-context identity.");
    }
    if (!journal) {
        return mutation_refusal(CBM_MUTATION_REFUSAL_AUTHORITY_UNAVAILABLE,
                                "Durable Union session authority is unavailable.");
    }
    CbmMutationSessionBinding binding = {0};
    bool other_context = false;
    CbmMutationJournalResult lookup = cbm_mutation_journal_find_session(
        journal, &attempt->host_context, &binding, &other_context);
    if (lookup == CBM_MUTATION_JOURNAL_ERR_NOT_FOUND) {
        return mutation_refusal(other_context ? CBM_MUTATION_REFUSAL_SESSION_STALE
                                              : CBM_MUTATION_REFUSAL_SESSION_UNBOUND,
                                other_context
                                    ? "No live Union session is bound to this host context."
                                    : "Open a grounded Union session bound to this host context.");
    }
    if (lookup != CBM_MUTATION_JOURNAL_OK) {
        return mutation_refusal(CBM_MUTATION_REFUSAL_AUTHORITY_UNAVAILABLE,
                                "Durable Union session authority could not be read.");
    }
    if (!cbm_change_grounding_is_valid(&binding.grounding)) {
        return mutation_refusal(CBM_MUTATION_REFUSAL_GROUNDING_MISSING,
                                "The live Union session has no valid scoped change grounding.");
    }
    if (!mutation_intent_covers_attempt(&binding.grounding, attempt)) {
        return mutation_refusal(CBM_MUTATION_REFUSAL_INTENT_SCOPE_MISMATCH,
                                "The repository write target or operation is outside the session's declared change intent.");
    }

    CbmMutationDecision decision = {0};
    CbmMutationJournalResult result = cbm_mutation_journal_commit_intent(
        journal, binding.horizon_id, attempt, &binding.grounding,
        decision.write_id, sizeof(decision.write_id));
    if (result != CBM_MUTATION_JOURNAL_OK) {
        return mutation_refusal(CBM_MUTATION_REFUSAL_JOURNAL_FAILURE,
                                "The write intent could not be committed durably.");
    }
    decision.permitted = true;
    decision.refusal.code = CBM_MUTATION_REFUSAL_NONE;
    return decision;
}
