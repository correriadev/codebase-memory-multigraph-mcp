/* Mutation gate domain contracts (E01). */
#ifndef CBM_UNION_MUTATION_GATE_H
#define CBM_UNION_MUTATION_GATE_H

#include <stdbool.h>
#include <stddef.h>

#define CBM_MUTATION_CONTEXT_ID_MAX 128
#define CBM_MUTATION_INTENT_KEY_MAX 128
#define CBM_MUTATION_REFERENCE_MAX 256
#define CBM_MUTATION_RATIONALE_MAX 512
#define CBM_MUTATION_REASON_MAX 256
#define CBM_MUTATION_WRITE_ID_MAX 128
#define CBM_MUTATION_TARGET_PATH_MAX 2048
#define CBM_MUTATION_CANONICAL_PATH_MAX 4096
#define CBM_MUTATION_SCOPE_MAX_TARGETS 4

typedef enum {
    CBM_MUTATION_HOST_UNKNOWN = 0,
    CBM_MUTATION_HOST_ANTIGRAVITY = 1,
    CBM_MUTATION_HOST_CODEX = 2
} CbmMutationHost;

typedef struct {
    CbmMutationHost host;
    char context_id[CBM_MUTATION_CONTEXT_ID_MAX];
} CbmHostWorkContext;

typedef enum {
    CBM_MUTATION_OPERATION_UNKNOWN = 0,
    CBM_MUTATION_OPERATION_READ,
    CBM_MUTATION_OPERATION_CREATE,
    CBM_MUTATION_OPERATION_MODIFY,
    CBM_MUTATION_OPERATION_DELETE,
    CBM_MUTATION_OPERATION_RENAME
} CbmMutationOperation;

typedef enum {
    CBM_MUTATION_SCOPE_UNKNOWN = 0,
    CBM_MUTATION_SCOPE_REPOSITORY,
    CBM_MUTATION_SCOPE_OUTSIDE
} CbmMutationScope;

typedef enum {
    CBM_MUTATION_EFFECT_READ = 0,
    CBM_MUTATION_EFFECT_REPOSITORY_WRITE,
    CBM_MUTATION_EFFECT_OUTSIDE_REPOSITORY,
    CBM_MUTATION_EFFECT_AMBIGUOUS
} CbmMutationEffect;

typedef struct {
    CbmHostWorkContext host_context;
    CbmMutationOperation operation;
    CbmMutationScope scope;
    char attempt_id[CBM_MUTATION_WRITE_ID_MAX];
    /* Canonical absolute primary target. For RENAME this is the source path. */
    char target_path[CBM_MUTATION_TARGET_PATH_MAX];
    /* Canonical absolute destination for RENAME; empty for other operations. */
    char secondary_target_path[CBM_MUTATION_TARGET_PATH_MAX];
} CbmMutationAttempt;

typedef enum {
    CBM_GROUNDING_NONE = 0,
    CBM_GROUNDING_CANON_CITATION,
    CBM_GROUNDING_DECLARED_INVENTION
} CbmGroundingKind;

typedef struct {
    CbmMutationOperation operation;
    char path[CBM_MUTATION_TARGET_PATH_MAX];
} CbmMutationIntentTarget;

typedef struct {
    CbmGroundingKind kind;
    char intent_key[CBM_MUTATION_INTENT_KEY_MAX];
    char reference[CBM_MUTATION_REFERENCE_MAX];
    char rationale[CBM_MUTATION_RATIONALE_MAX];
    /* Exact canonical path and operation pairs covered by this change intent. */
    size_t target_count;
    CbmMutationIntentTarget targets[CBM_MUTATION_SCOPE_MAX_TARGETS];
} CbmChangeGrounding;

typedef enum {
    CBM_MUTATION_REFUSAL_NONE = 0,
    CBM_MUTATION_REFUSAL_GROUNDING_MISSING,
    CBM_MUTATION_REFUSAL_SESSION_STALE,
    CBM_MUTATION_REFUSAL_SESSION_UNBOUND,
    CBM_MUTATION_REFUSAL_AUTHORITY_UNAVAILABLE,
    CBM_MUTATION_REFUSAL_EFFECT_UNKNOWN,
    CBM_MUTATION_REFUSAL_JOURNAL_FAILURE,
    CBM_MUTATION_REFUSAL_INTENT_SCOPE_MISMATCH
} CbmMutationRefusalCode;

typedef struct {
    CbmMutationRefusalCode code;
    char reason[CBM_MUTATION_REASON_MAX];
} CbmMutationRefusal;

typedef struct {
    bool permitted;
    CbmMutationRefusal refusal;
    char write_id[CBM_MUTATION_WRITE_ID_MAX];
} CbmMutationDecision;

struct CbmMutationJournal;

/* Classify using a scope already resolved against the canonical repository root.
 * Unknown operation or scope remains ambiguous and is never treated as a read. */
CbmMutationEffect cbm_mutation_classify(CbmMutationOperation operation,
                                        CbmMutationScope scope);

bool cbm_host_work_context_is_valid(const CbmHostWorkContext *context);
bool cbm_change_grounding_is_valid(const CbmChangeGrounding *grounding);
/* Resolve an absolute path, including a not-yet-created leaf whose parent
 * exists, to a canonical slash-normalized path. Relative and unresolved paths
 * are rejected. */
bool cbm_mutation_canonicalize_path(const char *path, char *canonical,
                                   size_t canonical_size);

/* Shared authorization path used by each host adapter. A permit is returned
 * only after the bound live session's grounding is validated and its durable
 * write intent is committed. */
CbmMutationDecision cbm_mutation_authorize_repository_write(
    struct CbmMutationJournal *journal, const CbmMutationAttempt *attempt);

#endif /* CBM_UNION_MUTATION_GATE_H */
