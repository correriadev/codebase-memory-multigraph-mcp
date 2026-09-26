/*
 * union_contest.h — Inter-Skill Contestation (Scope A10).
 *
 * Allows any archetype to challenge claims with evidence across horizons:
 * - INFORMATIVE: registers the question, claim remains admitted.
 * - BLOCKING: target marked CONTESTED; promotion blocked until resolved.
 * - INVALIDATING: triggers recall-candidate status / re-opening.
 * - Mandatory evidence: contest without evidence is refused (CONTEST_UNPROVEN).
 * - Resolution: revalidation or withdrawal with record.
 */
#ifndef CBM_UNION_CONTEST_H
#define CBM_UNION_CONTEST_H

#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_CONTEST_ID_MAX 64
#define CBM_CONTEST_EVIDENCE_CAP 8
#define CBM_CONTEST_EVIDENCE_LEN 128
#define CBM_CONTEST_REGISTRY_CAP 64

typedef enum {
    CBM_CONTEST_INFORMATIVE = 0,
    CBM_CONTEST_BLOCKING = 1,
    CBM_CONTEST_INVALIDATING = 2
} CbmContestSeverity;

const char *cbm_contest_severity_string(CbmContestSeverity severity);

typedef struct {
    char contest_id[CBM_CONTEST_ID_MAX];
    char submitter_identity[CBM_CONTEST_ID_MAX];
    char source_horizon[CBM_CONTEST_ID_MAX];
    char target_ref[CBM_CONTEST_EVIDENCE_LEN];
    CbmContestSeverity severity;

    char evidence[CBM_CONTEST_EVIDENCE_CAP][CBM_CONTEST_EVIDENCE_LEN];
    size_t evidence_count;

    bool resolved;
    char resolution_reason[CBM_CONTEST_EVIDENCE_LEN];
} CbmContestation;

typedef struct {
    CbmContestation contests[CBM_CONTEST_REGISTRY_CAP];
    size_t count;
} CbmContestRegistry;

void cbm_contest_registry_init(CbmContestRegistry *reg);

/* Submit a contestation. Requires evidence; otherwise refused with CONTEST_UNPROVEN. */
CbmRefusalCode cbm_contest_submit(CbmContestRegistry *reg,
                                 const CbmContestation *contest,
                                 char *out_reason, size_t reason_sz);

/* Query active blocking contests for a target. Returns count of blocking contests. */
size_t cbm_contest_is_blocked(const CbmContestRegistry *reg, const char *target_ref);

/* Resolve an active contest with recorded reason */
bool cbm_contest_resolve(CbmContestRegistry *reg, const char *contest_id, const char *resolution);

#endif /* CBM_UNION_CONTEST_H */
