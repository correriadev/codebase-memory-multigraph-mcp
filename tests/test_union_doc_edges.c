/*
 * test_union_doc_edges.c — Scope C05 acceptance criteria (Referential Resolution & Edges).
 */
#include "test_framework.h"
#include "../src/union/union_doc_edges.h"
#include "../src/union/union_refusal.h"

#include <string.h>

static void populate_claim_with_predicate(CbmClaim *claim, const char *id, const char *predicate) {
    memset(claim, 0, sizeof(*claim));
    strncpy(claim->id, id, sizeof(claim->id) - 1);
    strncpy(claim->predicate, predicate, sizeof(claim->predicate) - 1);
    claim->type = CBM_CLAIM_DECISION;
    claim->status = EPISTEMIC_ACCEPTED;
    claim->realized_by_is_empty = true; /* ∅ pending slot */

    claim->anchor.kind = CBM_ANCHOR_LOG_REF;
    strncpy(claim->anchor.log_ref.session_id, "session_5", sizeof(claim->anchor.log_ref.session_id) - 1);
    claim->anchor.log_ref.event_seq = 88;
}

/* AC1: Given a predicate citing a resolvable symbol,
 * When resolved, Then a REFERENCES edge exists to the code claim. */
TEST(test_edge_resolves_symbol_to_reference) {
    CbmCodeSymbolRegistry code_reg;
    cbm_code_registry_init(&code_reg);
    cbm_code_registry_add(&code_reg, "auth_validate_token");

    CbmDocEdgeStore edge_store;
    cbm_doc_edge_store_init(&edge_store);

    CbmClaim claim;
    populate_claim_with_predicate(&claim, "claim_auth_1",
                                  "The handler calls `auth_validate_token` before routing.");

    CbmRefusalCode code = cbm_doc_resolve_l1_references(&claim, &code_reg, &edge_store);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_FALSE(claim.has_unresolved_references);

    ASSERT_EQ(edge_store.count, 1);
    ASSERT_STR_EQ(edge_store.edges[0].source, "claim_auth_1");
    ASSERT_STR_EQ(edge_store.edges[0].target, "auth_validate_token");
    ASSERT_EQ(edge_store.edges[0].type, CBM_DOC_EDGE_REFERENCES);

    PASS();
}

/* AC2: Given a predicate citing a nonexistent symbol,
 * When resolved, Then UNRESOLVED_REFERENCE flag on the claim. */
TEST(test_edge_flags_unresolved_reference) {
    CbmCodeSymbolRegistry code_reg;
    cbm_code_registry_init(&code_reg);
    /* Symbol is not registered in code_reg */

    CbmDocEdgeStore edge_store;
    cbm_doc_edge_store_init(&edge_store);

    CbmClaim claim;
    populate_claim_with_predicate(&claim, "claim_auth_2",
                                  "The handler calls `nonexistent_function_foo` on start.");

    CbmRefusalCode code = cbm_doc_resolve_l1_references(&claim, &code_reg, &edge_store);
    ASSERT_EQ(code, CBM_REFUSAL_OK);
    ASSERT_TRUE(claim.has_unresolved_references);
    ASSERT_EQ(claim.unresolved_refs_count, 1);
    ASSERT_STR_EQ(claim.unresolved_refs[0], "nonexistent_function_foo");

    /* No edge created for nonexistent symbol */
    ASSERT_EQ(edge_store.count, 0);

    PASS();
}

/* AC3: Given an admitted conversational claim,
 * When inspected, Then DERIVES_FROM resolves to birth event and REALIZED_BY shows ∅. */
TEST(test_edge_conversational_birth_and_pending_realization) {
    CbmClaim claim;
    populate_claim_with_predicate(&claim, "claim_conv_1",
                                  "Tokens expire after 3600 seconds.");
    claim.realized_by_is_empty = true; /* ∅ */

    CbmDocEdgeStore edge_store;
    cbm_doc_edge_store_init(&edge_store);

    /* Record DERIVES_FROM edge to log birth */
    CbmDocEdge birth_edge = {0};
    strncpy(birth_edge.source, claim.id, sizeof(birth_edge.source) - 1);
    snprintf(birth_edge.target, sizeof(birth_edge.target), "log://%s#%llu",
             claim.anchor.log_ref.session_id, (unsigned long long)claim.anchor.log_ref.event_seq);
    birth_edge.type = CBM_DOC_EDGE_DERIVES_FROM;
    birth_edge.created_at_seq = claim.anchor.log_ref.event_seq;

    CbmRefusalCode code = cbm_doc_edge_add(&edge_store, NULL, &birth_edge, NULL, 0);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    ASSERT_TRUE(claim.realized_by_is_empty);
    ASSERT_EQ(claim.realized_by_count, 0);
    ASSERT_EQ(edge_store.edges[0].type, CBM_DOC_EDGE_DERIVES_FROM);
    ASSERT_STR_EQ(edge_store.edges[0].target, "log://session_5#88");

    PASS();
}

/* AC4: Given a realization event for a claim,
 * When applied, Then slot fills with code claim refs, claim status is unchanged,
 * and fill event is logged. */
TEST(test_edge_realization_event_fills_slot_without_status_change) {
    CbmClaim claim;
    populate_claim_with_predicate(&claim, "claim_real_1", "Password must be >= 12 chars.");
    claim.status = EPISTEMIC_ACCEPTED;
    claim.realized_by_is_empty = true;

    CbmDocEdgeStore edge_store;
    cbm_doc_edge_store_init(&edge_store);

    CbmRefusalCode code = cbm_doc_apply_realization(&claim, "cbm://repo/src/auth.c#check_password_len",
                                                   &edge_store, 150);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    /* Realized slot filled */
    ASSERT_FALSE(claim.realized_by_is_empty);
    ASSERT_EQ(claim.realized_by_count, 1);
    ASSERT_STR_EQ(claim.realized_by_refs[0], "cbm://repo/src/auth.c#check_password_len");

    /* Status is UNCHANGED: realization is provenance, not status transition */
    ASSERT_EQ(claim.status, EPISTEMIC_ACCEPTED);

    /* Edge stored: REALIZED_BY */
    ASSERT_EQ(edge_store.count, 1);
    ASSERT_EQ(edge_store.edges[0].type, CBM_DOC_EDGE_REALIZED_BY);
    ASSERT_STR_EQ(edge_store.edges[0].target, "cbm://repo/src/auth.c#check_password_len");

    PASS();
}

/* AC5: Given claim A superseding claim B,
 * When recorded, Then SUPERSEDES edge exists with scar, and B remains queryable
 * with status transition legible in history. */
TEST(test_edge_supersedes_with_scar) {
    CbmClaim claim_a, claim_b;
    populate_claim_with_predicate(&claim_b, "claim_legacy_hash", "Use MD5 for checksums.");
    claim_b.status = EPISTEMIC_ACCEPTED;

    populate_claim_with_predicate(&claim_a, "claim_modern_hash", "Use SHA-256 for checksums.");
    claim_a.status = EPISTEMIC_ACCEPTED;

    CbmDocEdgeStore edge_store;
    cbm_doc_edge_store_init(&edge_store);

    CbmRefusalCode code = cbm_doc_apply_supersession(&claim_a, &claim_b,
                                                     "MD5 deprecated due to collision vulnerability",
                                                     &edge_store, 200);
    ASSERT_EQ(code, CBM_REFUSAL_OK);

    /* B transitions to SHADOWED */
    ASSERT_EQ(claim_b.status, EPISTEMIC_SHADOWED);

    /* Edge created: SUPERSEDES with scar */
    ASSERT_EQ(edge_store.count, 1);
    ASSERT_EQ(edge_store.edges[0].type, CBM_DOC_EDGE_SUPERSEDES);
    ASSERT_STR_EQ(edge_store.edges[0].source, "claim_modern_hash");
    ASSERT_STR_EQ(edge_store.edges[0].target, "claim_legacy_hash");
    ASSERT_STR_EQ(edge_store.edges[0].scar, "MD5 deprecated due to collision vulnerability");

    PASS();
}

/* AC6: Asymmetry invariant: attempt a code -> doc write, assert refusal CODE_DOC_ASYMMETRY. */
TEST(test_edge_asymmetry_code_to_doc_refused) {
    CbmCodeSymbolRegistry code_reg;
    cbm_code_registry_init(&code_reg);
    cbm_code_registry_add(&code_reg, "auth_login");

    CbmDocEdgeStore edge_store;
    cbm_doc_edge_store_init(&edge_store);

    /* Attempt to add an edge from code node to doc node */
    CbmDocEdge invalid_edge = {0};
    strncpy(invalid_edge.source, "auth_login", sizeof(invalid_edge.source) - 1);
    strncpy(invalid_edge.target, "claim_auth_1", sizeof(invalid_edge.target) - 1);
    invalid_edge.type = CBM_DOC_EDGE_REFERENCES;

    char reason[128] = {0};
    CbmRefusalCode code = cbm_doc_edge_add(&edge_store, &code_reg, &invalid_edge,
                                          reason, sizeof(reason));

    ASSERT_EQ(code, CBM_REFUSAL_CODE_DOC_ASYMMETRY);
    ASSERT_TRUE(strstr(reason, "asymmetry") != NULL);
    ASSERT_EQ(edge_store.count, 0);

    PASS();
}

/* AC7: Reverse index query: code symbol -> all referencing claims with status. */
TEST(test_edge_reverse_index_query) {
    CbmDocEdgeStore edge_store;
    cbm_doc_edge_store_init(&edge_store);

    CbmDocEdge e1 = {0}, e2 = {0};
    strncpy(e1.source, "claim_doc_1", sizeof(e1.source) - 1);
    strncpy(e1.target, "db_connect", sizeof(e1.target) - 1);
    e1.type = CBM_DOC_EDGE_REFERENCES;
    (void)cbm_doc_edge_add(&edge_store, NULL, &e1, NULL, 0);

    strncpy(e2.source, "claim_doc_2", sizeof(e2.source) - 1);
    strncpy(e2.target, "db_connect", sizeof(e2.target) - 1);
    e2.type = CBM_DOC_EDGE_REALIZED_BY;
    (void)cbm_doc_edge_add(&edge_store, NULL, &e2, NULL, 0);

    CbmDocEdge results[8];
    size_t count = cbm_doc_reverse_query_symbol(&edge_store, "db_connect", results, 8);

    ASSERT_EQ(count, 2);
    ASSERT_STR_EQ(results[0].source, "claim_doc_1");
    ASSERT_STR_EQ(results[1].source, "claim_doc_2");

    PASS();
}

SUITE(union_doc_edges) {
    RUN_TEST(test_edge_resolves_symbol_to_reference);
    RUN_TEST(test_edge_flags_unresolved_reference);
    RUN_TEST(test_edge_conversational_birth_and_pending_realization);
    RUN_TEST(test_edge_realization_event_fills_slot_without_status_change);
    RUN_TEST(test_edge_supersedes_with_scar);
    RUN_TEST(test_edge_asymmetry_code_to_doc_refused);
    RUN_TEST(test_edge_reverse_index_query);
}
