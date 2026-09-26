/*
 * union_contest.c — Inter-Skill Contestation (Scope A10).
 */
#include "union_contest.h"

#include <stdio.h>
#include <string.h>

const char *cbm_contest_severity_string(CbmContestSeverity severity) {
    switch (severity) {
        case CBM_CONTEST_INFORMATIVE: return "informative";
        case CBM_CONTEST_BLOCKING: return "blocking";
        case CBM_CONTEST_INVALIDATING: return "invalidating";
        default: return "unknown";
    }
}

void cbm_contest_registry_init(CbmContestRegistry *reg) {
    if (!reg) return;
    memset(reg, 0, sizeof(*reg));
}

CbmRefusalCode cbm_contest_submit(CbmContestRegistry *reg,
                                 const CbmContestation *contest,
                                 char *out_reason, size_t reason_sz) {
    if (out_reason && reason_sz > 0) out_reason[0] = '\0';
    if (!reg || !contest || !contest->target_ref[0]) {
        return CBM_REFUSAL_CONTRACT_INVALID;
    }

    /* Invariant AC1: contest without evidence is refused CONTEST_UNPROVEN */
    if (contest->evidence_count == 0) {
        if (out_reason && reason_sz > 0) {
            snprintf(out_reason, reason_sz, "evidence_required");
        }
        cbm_refusal_emit(CBM_REFUSAL_CONTEST_UNPROVEN, contest->source_horizon, "evidence_required");
        return CBM_REFUSAL_CONTEST_UNPROVEN;
    }

    if (reg->count >= CBM_CONTEST_REGISTRY_CAP) {
        if (out_reason && reason_sz > 0) {
            snprintf(out_reason, reason_sz, "contest_registry_full");
        }
        return CBM_REFUSAL_SCOPE_EXCEEDED;
    }

    reg->contests[reg->count] = *contest;
    reg->contests[reg->count].resolved = false;
    reg->contests[reg->count].resolution_reason[0] = '\0';

    fprintf(stderr, "level=info msg=union.contest.submitted contest_id=%s target=%s severity=%s\n",
            contest->contest_id, contest->target_ref, cbm_contest_severity_string(contest->severity));

    if (contest->severity == CBM_CONTEST_INVALIDATING) {
        fprintf(stderr, "level=warn msg=union.contest.recall_candidate target=%s contest_id=%s\n",
                contest->target_ref, contest->contest_id);
    }

    reg->count++;
    return CBM_REFUSAL_OK;
}

size_t cbm_contest_is_blocked(const CbmContestRegistry *reg, const char *target_ref) {
    if (!reg || !target_ref || !target_ref[0]) return 0;
    size_t count = 0;
    for (size_t i = 0; i < reg->count; i++) {
        if (!reg->contests[i].resolved &&
            reg->contests[i].severity == CBM_CONTEST_BLOCKING &&
            strcmp(reg->contests[i].target_ref, target_ref) == 0) {
            count++;
        }
    }
    return count;
}

bool cbm_contest_resolve(CbmContestRegistry *reg, const char *contest_id, const char *resolution) {
    if (!reg || !contest_id || !contest_id[0]) return false;

    for (size_t i = 0; i < reg->count; i++) {
        if (strcmp(reg->contests[i].contest_id, contest_id) == 0) {
            reg->contests[i].resolved = true;
            if (resolution) {
                snprintf(reg->contests[i].resolution_reason,
                         sizeof(reg->contests[i].resolution_reason), "%s", resolution);
            }
            fprintf(stderr, "level=info msg=union.contest.resolved contest_id=%s reason=%s\n",
                    contest_id, resolution ? resolution : "resolved");
            return true;
        }
    }
    return false;
}
