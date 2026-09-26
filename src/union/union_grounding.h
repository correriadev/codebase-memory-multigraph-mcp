/*
 * union_grounding.h — Epistemic-Status Grounding Reads (Scope A03).
 *
 * Implements grounding read results with epistemic status, provenance tracking,
 * speculative content isolation, and contestation awareness.
 */
#ifndef CBM_UNION_GROUNDING_H
#define CBM_UNION_GROUNDING_H

#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_GROUNDING_URI_MAX 128
#define CBM_GROUNDING_PROVENANCE_MAX 128
#define CBM_GROUNDING_STATUS_MAX 32
#define CBM_GROUNDING_SEQ_MAX 64
#define CBM_GROUNDING_ITEMS_CAP 32

typedef struct {
    char uri[CBM_GROUNDING_URI_MAX];
    char epistemic_status[CBM_GROUNDING_STATUS_MAX]; /* "ADMITTED", "PROPOSED", "CONTESTED", "SUPERSEDED" */
    char provenance_ref[CBM_GROUNDING_PROVENANCE_MAX];
    char based_on_seq[CBM_GROUNDING_SEQ_MAX];
    bool provenance_missing;
    bool is_contested;
    char contest_ref[CBM_GROUNDING_PROVENANCE_MAX];
    char contest_severity[32];
} CbmGroundingItem;

typedef struct {
    CbmGroundingItem items[CBM_GROUNDING_ITEMS_CAP];
    size_t count;
} CbmGroundingResultSet;

void cbm_grounding_init(CbmGroundingResultSet *rs);

/* Add item to result set. Automatically flags PROVENANCE_MISSING if provenance_ref is empty. */
bool cbm_grounding_add_item(CbmGroundingResultSet *rs,
                            const char *uri,
                            const char *epistemic_status,
                            const char *provenance_ref,
                            const char *based_on_seq,
                            const char *contest_ref,
                            const char *contest_severity);

/* Filter: when include_speculative is false, items marked "PROPOSED" are excluded (A03 AC2). */
void cbm_grounding_filter(const CbmGroundingResultSet *src,
                          bool include_speculative,
                          CbmGroundingResultSet *dest);

#endif /* CBM_UNION_GROUNDING_H */
