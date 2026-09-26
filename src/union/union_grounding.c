/*
 * union_grounding.c — Epistemic-Status Grounding Reads (Scope A03).
 */
#include "union_grounding.h"

#include <stdio.h>
#include <string.h>

void cbm_grounding_init(CbmGroundingResultSet *rs) {
    if (!rs) return;
    memset(rs, 0, sizeof(*rs));
}

bool cbm_grounding_add_item(CbmGroundingResultSet *rs,
                            const char *uri,
                            const char *epistemic_status,
                            const char *provenance_ref,
                            const char *based_on_seq,
                            const char *contest_ref,
                            const char *contest_severity) {
    if (!rs || !uri || !uri[0] || rs->count >= CBM_GROUNDING_ITEMS_CAP) {
        return false;
    }

    CbmGroundingItem *item = &rs->items[rs->count];
    memset(item, 0, sizeof(*item));

    snprintf(item->uri, sizeof(item->uri), "%s", uri);
    snprintf(item->epistemic_status, sizeof(item->epistemic_status), "%s",
             epistemic_status ? epistemic_status : "ADMITTED");

    if (provenance_ref && provenance_ref[0]) {
        snprintf(item->provenance_ref, sizeof(item->provenance_ref), "%s", provenance_ref);
        item->provenance_missing = false;
    } else {
        item->provenance_missing = true;
    }

    if (based_on_seq && based_on_seq[0]) {
        snprintf(item->based_on_seq, sizeof(item->based_on_seq), "%s", based_on_seq);
    }

    if (contest_ref && contest_ref[0]) {
        item->is_contested = true;
        snprintf(item->contest_ref, sizeof(item->contest_ref), "%s", contest_ref);
        if (contest_severity && contest_severity[0]) {
            snprintf(item->contest_severity, sizeof(item->contest_severity), "%s", contest_severity);
        }
    }

    rs->count++;
    return true;
}

void cbm_grounding_filter(const CbmGroundingResultSet *src,
                          bool include_speculative,
                          CbmGroundingResultSet *dest) {
    if (!src || !dest) return;
    cbm_grounding_init(dest);

    for (size_t i = 0; i < src->count; i++) {
        const CbmGroundingItem *it = &src->items[i];
        if (!include_speculative && strcmp(it->epistemic_status, "PROPOSED") == 0) {
            continue; /* Exclude speculative content from base grounding reads */
        }
        if (dest->count < CBM_GROUNDING_ITEMS_CAP) {
            dest->items[dest->count++] = *it;
        }
    }
}
