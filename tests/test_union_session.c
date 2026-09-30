/*
 * test_union_session.c — Scope A01 acceptance criteria.
 *
 * Uses a real HorizonConnectionPool on a temp dir: the protocol wraps the
 * existing horizon machinery, it does not mock it.
 */
#include "test_framework.h"
#include "test_helpers.h"
#include "../src/union/mutation_gate.h"
#include "../src/union/mutation_journal.h"
#include "../src/union/union_session.h"
#include "../src/foundation/compat.h"

#include <sqlite3.h>

#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
#endif

typedef struct {
    HorizonConnectionPool pool;
    CbmContractRegistry contracts;
    CbmSessionRegistry sessions;
    char dir[128];
} SessionFixture;

/* Returns 0 on success. */
static int session_fixture_init(SessionFixture *fx) {
    memset(fx, 0, sizeof(*fx));
    snprintf(fx->dir, sizeof(fx->dir), "/tmp/cbm_union_session_XXXXXX");
    if (cbm_mkdtemp(fx->dir) == NULL) return -1;

    if (cbm_horizon_pool_init(&fx->pool, fx->dir) != 0) return -1;
    cbm_contract_registry_init(&fx->contracts);
    cbm_session_registry_init(&fx->sessions);

    CbmSkillContract contract;
    memset(&contract, 0, sizeof(contract));
    snprintf(contract.identity, sizeof(contract.identity), "graph_grounding");
    snprintf(contract.territory[0], sizeof(contract.territory[0]), "context_read");
    contract.territory_count = 1;
    contract.effect_class = CBM_EFFECT_IDEMPOTENT;
    contract.acknowledged_refusals_mask =
        (1u << CBM_REFUSAL_ANCHOR_NOT_FOUND) | (1u << CBM_REFUSAL_STALE_BASE);
    contract.requires_trace = true;
    contract.requires_exclusion_summary = true;

    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    if (cbm_contract_registry_put(&fx->contracts, &contract, err, sizeof(err)) != CBM_CONTRACT_OK) {
        return -1;
    }
    return 0;
}

static void session_fixture_cleanup(SessionFixture *fx) {
    cbm_horizon_pool_close_all(&fx->pool);
    th_rmtree(fx->dir);
}

static bool session_test_set_target(SessionFixture *fx, CbmMutationAttempt *attempt,
                                    CbmChangeGrounding *grounding,
                                    CbmMutationOperation operation, const char *leaf) {
    char requested[CBM_MUTATION_CANONICAL_PATH_MAX];
    int written = snprintf(requested, sizeof(requested), "%s/%s", fx->dir, leaf);
    if (written <= 0 || (size_t)written >= sizeof(requested) ||
        !cbm_mutation_canonicalize_path(requested, attempt->target_path,
                                        sizeof(attempt->target_path))) {
        return false;
    }
    grounding->target_count = 1U;
    grounding->targets[0].operation = operation;
    snprintf(grounding->targets[0].path, sizeof(grounding->targets[0].path), "%s",
             attempt->target_path);
    return strlen(attempt->target_path) < sizeof(grounding->targets[0].path);
}

static bool session_test_set_rename_targets(SessionFixture *fx, CbmMutationAttempt *attempt,
                                            CbmChangeGrounding *grounding,
                                            const char *source_leaf,
                                            const char *destination_leaf) {
    char source_request[CBM_MUTATION_CANONICAL_PATH_MAX];
    char destination_request[CBM_MUTATION_CANONICAL_PATH_MAX];
    int source_written = snprintf(source_request, sizeof(source_request), "%s/%s",
                                  fx->dir, source_leaf);
    int destination_written = snprintf(destination_request, sizeof(destination_request), "%s/%s",
                                       fx->dir, destination_leaf);
    if (source_written <= 0 || (size_t)source_written >= sizeof(source_request) ||
        destination_written <= 0 || (size_t)destination_written >= sizeof(destination_request) ||
        !cbm_mutation_canonicalize_path(source_request, attempt->target_path,
                                        sizeof(attempt->target_path)) ||
        !cbm_mutation_canonicalize_path(destination_request,
                                        attempt->secondary_target_path,
                                        sizeof(attempt->secondary_target_path)) ||
        strcmp(attempt->target_path, attempt->secondary_target_path) == 0) {
        return false;
    }
    attempt->operation = CBM_MUTATION_OPERATION_RENAME;
    grounding->target_count = 2U;
    for (size_t index = 0; index < grounding->target_count; index++) {
        grounding->targets[index].operation = CBM_MUTATION_OPERATION_RENAME;
    }
    snprintf(grounding->targets[0].path, sizeof(grounding->targets[0].path), "%s",
             attempt->target_path);
    snprintf(grounding->targets[1].path, sizeof(grounding->targets[1].path), "%s",
             attempt->secondary_target_path);
    return true;
}

/* AC1: registered identity opens full-mode; the host log records identity,
 * contract, based_on_seq (asserted via returned state + open event fields). */
TEST(test_session_open_registered) {
    SessionFixture fx;
    if (session_fixture_init(&fx) != 0) FAIL("fixture init");

    CbmSessionHorizon session;
    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    int rc = cbm_session_open(&fx.sessions, &fx.contracts, &fx.pool, 4242,
                             "graph_grounding", "graph_grounding", "seq_41",
                             &session, err, sizeof(err));
    ASSERT_EQ(rc, CBM_SESSION_OK);
    ASSERT_FALSE(session.restricted);
    ASSERT_EQ(session.open_refusal, CBM_REFUSAL_OK);
    ASSERT_STR_EQ(session.identity, "graph_grounding");
    ASSERT_STR_EQ(session.based_on_seq, "seq_41");
    ASSERT(session.horizon_id[0] != '\0');

    /* State is queryable mid-lifecycle. */
    const CbmSessionHorizon *got = cbm_session_get(&fx.sessions, session.horizon_id);
    ASSERT_NOT_NULL(got);
    ASSERT_STR_EQ(got->contract_id, "graph_grounding");

    /* The horizon DB physically exists (protocol wraps the pool, not a mock). */
    sqlite3 *db = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&fx.pool, session.horizon_id, &db), 0);
    ASSERT_NOT_NULL(db);

    session_fixture_cleanup(&fx);
    PASS();
}

/* AC2: unregistered identity opens RESTRICTED — recorded, not refused. */
TEST(test_session_open_unregistered_restricted) {
    SessionFixture fx;
    if (session_fixture_init(&fx) != 0) FAIL("fixture init");

    CbmSessionHorizon session;
    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    int rc = cbm_session_open(&fx.sessions, &fx.contracts, &fx.pool, 4242,
                             "mystery_skill", "mystery_skill", "0",
                             &session, err, sizeof(err));
    ASSERT_EQ(rc, CBM_SESSION_OK);
    ASSERT_TRUE(session.restricted);
    ASSERT_EQ(session.open_refusal, CBM_REFUSAL_CONTRACT_UNKNOWN);
    ASSERT_STR_EQ(err, ""); /* restricted mode is not an error return */

    /* Missing contract id entirely: also restricted. */
    CbmSessionHorizon session2;
    rc = cbm_session_open(&fx.sessions, &fx.contracts, &fx.pool, 4242,
                          "another_skill", NULL, "0", &session2, err, sizeof(err));
    ASSERT_EQ(rc, CBM_SESSION_OK);
    ASSERT_TRUE(session2.restricted);

    session_fixture_cleanup(&fx);
    PASS();
}

/* AC3: the recorded based_on_seq is verbatim; no silent re-basing. */
TEST(test_session_based_on_seq_recorded) {
    SessionFixture fx;
    if (session_fixture_init(&fx) != 0) FAIL("fixture init");

    CbmSessionHorizon session;
    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    int rc = cbm_session_open(&fx.sessions, &fx.contracts, &fx.pool, 4242,
                              "graph_grounding", "graph_grounding", NULL,
                              &session, err, sizeof(err));
    ASSERT_EQ(rc, CBM_SESSION_OK);
    ASSERT_STR_EQ(session.based_on_seq, "0"); /* NULL defaults to "0", never blank */

    /* Verify the horizon metadata row carries the same based_on_seq. */
    sqlite3 *db = NULL;
    ASSERT_EQ(cbm_horizon_pool_get(&fx.pool, session.horizon_id, &db), 0);
    sqlite3_stmt *stmt = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(db, "SELECT based_on_seq FROM horizon_metadata WHERE horizon_id = ?",
                                -1, &stmt, NULL), SQLITE_OK);
    sqlite3_bind_text(stmt, 1, session.horizon_id, -1, SQLITE_STATIC);
    ASSERT_EQ(sqlite3_step(stmt), SQLITE_ROW);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(stmt, 0), "0");
    sqlite3_finalize(stmt);

    session_fixture_cleanup(&fx);
    PASS();
}

/* Counters feed the closure event (ledger integration is A06; counters are
 * the session's own honest record). */
TEST(test_session_counters_reach_closure) {
    SessionFixture fx;
    if (session_fixture_init(&fx) != 0) FAIL("fixture init");

    CbmSessionHorizon session;
    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    ASSERT_EQ(cbm_session_open(&fx.sessions, &fx.contracts, &fx.pool, 4242,
                              "graph_grounding", "graph_grounding", "seq_41",
                              &session, err, sizeof(err)), CBM_SESSION_OK);

    ASSERT_EQ(cbm_session_record_action(&fx.sessions, session.horizon_id), CBM_SESSION_OK);
    ASSERT_EQ(cbm_session_record_action(&fx.sessions, session.horizon_id), CBM_SESSION_OK);
    ASSERT_EQ(cbm_session_record_refusal(&fx.sessions, session.horizon_id), CBM_SESSION_OK);

    /* Unknown session id: honest not-found, not silent success. */
    ASSERT_EQ(cbm_session_record_action(&fx.sessions, "horizon_nope"), CBM_SESSION_ERR_NOT_FOUND);

    CbmSessionClosure closure;
    ASSERT_EQ(cbm_session_close(&fx.sessions, &fx.pool, session.horizon_id,
                               CBM_SESSION_CLOSE_NORMAL, &closure, err, sizeof(err)),
              CBM_SESSION_OK);
    ASSERT_EQ(closure.actions, 2);
    ASSERT_EQ(closure.refusals, 1);
    ASSERT_STR_EQ(closure.reason, "NORMAL");
    ASSERT_STR_EQ(closure.identity, "graph_grounding");

    session_fixture_cleanup(&fx);
    PASS();
}

/* AC4: closure destroys the content; the session leaves the registry. */
TEST(test_session_close_destroys_content) {
    SessionFixture fx;
    if (session_fixture_init(&fx) != 0) FAIL("fixture init");

    CbmSessionHorizon session;
    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    ASSERT_EQ(cbm_session_open(&fx.sessions, &fx.contracts, &fx.pool, 4242,
                              "graph_grounding", "graph_grounding", "seq_41",
                              &session, err, sizeof(err)), CBM_SESSION_OK);
    char horizon_id[CBM_HORIZON_ID_MAX];
    snprintf(horizon_id, sizeof(horizon_id), "%s", session.horizon_id);

    size_t count_before = fx.sessions.count;
    ASSERT_EQ(cbm_session_close(&fx.sessions, &fx.pool, horizon_id,
                               CBM_SESSION_CLOSE_ABNORMAL, NULL, err, sizeof(err)),
              CBM_SESSION_OK);
    ASSERT_EQ(fx.sessions.count, count_before - 1);

    /* Content is gone: query returns nothing. */
    ASSERT_NULL(cbm_session_get(&fx.sessions, horizon_id));

    /* Double close: honest not-found, never silent success. */
    ASSERT_EQ(cbm_session_close(&fx.sessions, &fx.pool, horizon_id,
                               CBM_SESSION_CLOSE_NORMAL, NULL, err, sizeof(err)),
              CBM_SESSION_ERR_NOT_FOUND);

    session_fixture_cleanup(&fx);
    PASS();
}

/* AC5: an empty session closes as a first-class EMPTY outcome. */
TEST(test_session_empty_close_first_class) {
    SessionFixture fx;
    if (session_fixture_init(&fx) != 0) FAIL("fixture init");

    CbmSessionHorizon session;
    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    ASSERT_EQ(cbm_session_open(&fx.sessions, &fx.contracts, &fx.pool, 4242,
                              "graph_grounding", "graph_grounding", "seq_41",
                              &session, err, sizeof(err)), CBM_SESSION_OK);

    CbmSessionClosure closure;
    ASSERT_EQ(cbm_session_close(&fx.sessions, &fx.pool, session.horizon_id,
                               CBM_SESSION_CLOSE_EMPTY, &closure, err, sizeof(err)),
              CBM_SESSION_OK);
    ASSERT_STR_EQ(closure.reason, "EMPTY");
    ASSERT_EQ(closure.actions, 0);
    ASSERT_EQ(closure.refusals, 0);

    session_fixture_cleanup(&fx);
    PASS();
}

/* Registry capacity is bounded and honest. */
TEST(test_session_registry_full) {
    SessionFixture fx;
    if (session_fixture_init(&fx) != 0) FAIL("fixture init");

    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    for (size_t i = 0; i < CBM_SESSION_REGISTRY_CAP; i++) {
        CbmSessionHorizon session;
        ASSERT_EQ(cbm_session_open(&fx.sessions, &fx.contracts, &fx.pool, 4242,
                                  "graph_grounding", "graph_grounding", "seq_41",
                                  &session, err, sizeof(err)), CBM_SESSION_OK);
    }

    CbmSessionHorizon overflow;
    ASSERT_EQ(cbm_session_open(&fx.sessions, &fx.contracts, &fx.pool, 4242,
                              "graph_grounding", "graph_grounding", "seq_41",
                              &overflow, err, sizeof(err)), CBM_SESSION_ERR_FULL);
    ASSERT_STR_EQ(err, "SESSION_REGISTRY_FULL");

    session_fixture_cleanup(&fx);
    PASS();
}

TEST(test_mutation_effect_classification) {
    ASSERT_EQ(cbm_mutation_classify(CBM_MUTATION_OPERATION_READ,
                                    CBM_MUTATION_SCOPE_UNKNOWN),
              CBM_MUTATION_EFFECT_READ);
    ASSERT_EQ(cbm_mutation_classify(CBM_MUTATION_OPERATION_CREATE,
                                    CBM_MUTATION_SCOPE_REPOSITORY),
              CBM_MUTATION_EFFECT_REPOSITORY_WRITE);
    ASSERT_EQ(cbm_mutation_classify(CBM_MUTATION_OPERATION_MODIFY,
                                    CBM_MUTATION_SCOPE_REPOSITORY),
              CBM_MUTATION_EFFECT_REPOSITORY_WRITE);
    ASSERT_EQ(cbm_mutation_classify(CBM_MUTATION_OPERATION_DELETE,
                                    CBM_MUTATION_SCOPE_REPOSITORY),
              CBM_MUTATION_EFFECT_REPOSITORY_WRITE);
    ASSERT_EQ(cbm_mutation_classify(CBM_MUTATION_OPERATION_RENAME,
                                    CBM_MUTATION_SCOPE_REPOSITORY),
              CBM_MUTATION_EFFECT_REPOSITORY_WRITE);
    ASSERT_EQ(cbm_mutation_classify(CBM_MUTATION_OPERATION_CREATE,
                                    CBM_MUTATION_SCOPE_OUTSIDE),
              CBM_MUTATION_EFFECT_OUTSIDE_REPOSITORY);
    ASSERT_EQ(cbm_mutation_classify(CBM_MUTATION_OPERATION_UNKNOWN,
                                    CBM_MUTATION_SCOPE_REPOSITORY),
              CBM_MUTATION_EFFECT_AMBIGUOUS);
    ASSERT_EQ(cbm_mutation_classify(CBM_MUTATION_OPERATION_MODIFY,
                                    CBM_MUTATION_SCOPE_UNKNOWN),
              CBM_MUTATION_EFFECT_AMBIGUOUS);
    PASS();
}

TEST(test_mutation_host_context_requires_stable_identity) {
    CbmHostWorkContext context = {0};
    context.host = CBM_MUTATION_HOST_CODEX;
    ASSERT_FALSE(cbm_host_work_context_is_valid(&context));

    snprintf(context.context_id, sizeof(context.context_id), "turn_42");
    ASSERT_TRUE(cbm_host_work_context_is_valid(&context));

    context.host = CBM_MUTATION_HOST_UNKNOWN;
    ASSERT_FALSE(cbm_host_work_context_is_valid(&context));
    PASS();
}

TEST(test_change_grounding_requires_intent_and_provenance) {
    SessionFixture fx;
    if (session_fixture_init(&fx) != 0) FAIL("fixture init");
    CbmChangeGrounding grounding = {0};
    CbmMutationAttempt attempt = {0};
    attempt.operation = CBM_MUTATION_OPERATION_MODIFY;
    grounding.kind = CBM_GROUNDING_CANON_CITATION;
    snprintf(grounding.intent_key, sizeof(grounding.intent_key), "change_1");
    ASSERT_FALSE(cbm_change_grounding_is_valid(&grounding));

    snprintf(grounding.reference, sizeof(grounding.reference), "@inst/clean-arch#rule-1");
    ASSERT_TRUE(session_test_set_target(&fx, &attempt, &grounding,
                                        CBM_MUTATION_OPERATION_MODIFY, "parser.c"));
    ASSERT_TRUE(cbm_change_grounding_is_valid(&grounding));
    snprintf(grounding.rationale, sizeof(grounding.rationale), "Must choose one provenance.");
    ASSERT_FALSE(cbm_change_grounding_is_valid(&grounding));

    memset(&grounding, 0, sizeof(grounding));
    grounding.kind = CBM_GROUNDING_DECLARED_INVENTION;
    snprintf(grounding.intent_key, sizeof(grounding.intent_key), "change_2");
    snprintf(grounding.rationale, sizeof(grounding.rationale), "Operator requested this design.");
    ASSERT_TRUE(session_test_set_target(&fx, &attempt, &grounding,
                                        CBM_MUTATION_OPERATION_MODIFY, "parser.c"));
    ASSERT_TRUE(cbm_change_grounding_is_valid(&grounding));

    grounding.rationale[0] = '\0';
    ASSERT_FALSE(cbm_change_grounding_is_valid(&grounding));
    session_fixture_cleanup(&fx);
    PASS();
}

TEST(test_mutation_rename_authorizes_and_journals_both_endpoints) {
    SessionFixture fx;
    if (session_fixture_init(&fx) != 0) FAIL("fixture init");

    CbmMutationAttempt attempt = {0};
    attempt.host_context.host = CBM_MUTATION_HOST_CODEX;
    snprintf(attempt.host_context.context_id, sizeof(attempt.host_context.context_id),
             "thread_rename_42");
    attempt.scope = CBM_MUTATION_SCOPE_REPOSITORY;
    snprintf(attempt.attempt_id, sizeof(attempt.attempt_id), "rename_call_1");
    CbmChangeGrounding grounding = {0};
    grounding.kind = CBM_GROUNDING_DECLARED_INVENTION;
    snprintf(grounding.intent_key, sizeof(grounding.intent_key), "rename_source_file");
    snprintf(grounding.rationale, sizeof(grounding.rationale),
             "The operator requested this file move.");
    ASSERT_TRUE(session_test_set_rename_targets(&fx, &attempt, &grounding,
                                                "source.c", "destination.c"));
    ASSERT_TRUE(cbm_change_grounding_is_valid(&grounding));

    char journal_path[256];
    snprintf(journal_path, sizeof(journal_path), "%s/rename.sqlite", fx.dir);
    CbmMutationJournal journal = {0};
    ASSERT_EQ(cbm_mutation_journal_open(&journal, journal_path), CBM_MUTATION_JOURNAL_OK);
    ASSERT_EQ(cbm_mutation_journal_register_session(&journal, "horizon_rename",
                                                    &attempt.host_context, &grounding),
              CBM_MUTATION_JOURNAL_OK);

    CbmMutationDecision decision = cbm_mutation_authorize_repository_write(&journal, &attempt);
    ASSERT_TRUE(decision.permitted);
    ASSERT_TRUE(decision.write_id[0] != '\0');
    sqlite3_stmt *statement = NULL;
    ASSERT_EQ(sqlite3_prepare_v2(
                  journal.db,
                  "SELECT target_path,secondary_target_path FROM mutation_write_journal WHERE write_id=?;",
                  -1, &statement, NULL), SQLITE_OK);
    ASSERT_EQ(sqlite3_bind_text(statement, 1, decision.write_id, -1, SQLITE_TRANSIENT), SQLITE_OK);
    ASSERT_EQ(sqlite3_step(statement), SQLITE_ROW);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(statement, 0), attempt.target_path);
    ASSERT_STR_EQ((const char *)sqlite3_column_text(statement, 1),
                  attempt.secondary_target_path);
    sqlite3_finalize(statement);

    char unscoped_request[CBM_MUTATION_CANONICAL_PATH_MAX];
    char unscoped_path[CBM_MUTATION_TARGET_PATH_MAX];
    int unscoped_written = snprintf(unscoped_request, sizeof(unscoped_request),
                                    "%s/unscoped.c", fx.dir);
    ASSERT_TRUE(unscoped_written > 0 && (size_t)unscoped_written < sizeof(unscoped_request));
    ASSERT_TRUE(cbm_mutation_canonicalize_path(unscoped_request, unscoped_path,
                                               sizeof(unscoped_path)));
    snprintf(attempt.secondary_target_path, sizeof(attempt.secondary_target_path), "%s",
             unscoped_path);
    decision = cbm_mutation_authorize_repository_write(&journal, &attempt);
    ASSERT_FALSE(decision.permitted);
    ASSERT_EQ(decision.refusal.code, CBM_MUTATION_REFUSAL_INTENT_SCOPE_MISMATCH);

    ASSERT_EQ(cbm_mutation_journal_mark_session_unknown(&journal, "horizon_rename"),
              CBM_MUTATION_JOURNAL_OK);
    cbm_mutation_journal_close(&journal);
    session_fixture_cleanup(&fx);
    PASS();
}

TEST(test_session_mutation_context_is_bound_and_not_replayable) {
    SessionFixture fx;
    if (session_fixture_init(&fx) != 0) FAIL("fixture init");

    CbmSessionHorizon session;
    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    ASSERT_EQ(cbm_session_open(&fx.sessions, &fx.contracts, &fx.pool, 4242,
                              "graph_grounding", "graph_grounding", "seq_41",
                              &session, err, sizeof(err)), CBM_SESSION_OK);

    CbmHostWorkContext context = {0};
    context.host = CBM_MUTATION_HOST_CODEX;
    snprintf(context.context_id, sizeof(context.context_id), "thread_42");
    CbmChangeGrounding grounding = {0};
    grounding.kind = CBM_GROUNDING_DECLARED_INVENTION;
    snprintf(grounding.intent_key, sizeof(grounding.intent_key), "change_42");
    snprintf(grounding.rationale, sizeof(grounding.rationale), "Requested new behavior.");
    CbmMutationAttempt scoped_attempt = {0};
    scoped_attempt.operation = CBM_MUTATION_OPERATION_MODIFY;
    ASSERT_TRUE(session_test_set_target(&fx, &scoped_attempt, &grounding,
                                        CBM_MUTATION_OPERATION_MODIFY, "change.c"));
    ASSERT_EQ(cbm_session_bind_mutation_context(&fx.sessions, session.horizon_id,
                                                &context, &grounding, err, sizeof(err)),
              CBM_SESSION_OK);

    const CbmSessionHorizon *bound = cbm_session_find_bound_context(&fx.sessions, &context);
    ASSERT_NOT_NULL(bound);
    ASSERT_STR_EQ(bound->horizon_id, session.horizon_id);
    ASSERT_TRUE(bound->has_bound_context);
    ASSERT_TRUE(bound->has_change_grounding);

    CbmHostWorkContext replay = context;
    snprintf(replay.context_id, sizeof(replay.context_id), "thread_99");
    ASSERT_NULL(cbm_session_find_bound_context(&fx.sessions, &replay));
    ASSERT_EQ(cbm_session_bind_mutation_context(&fx.sessions, session.horizon_id,
                                                &replay, &grounding, err, sizeof(err)),
              CBM_SESSION_ERR_CONTEXT_ALREADY_BOUND);

    ASSERT_EQ(cbm_session_close(&fx.sessions, &fx.pool, session.horizon_id,
                                CBM_SESSION_CLOSE_NORMAL, NULL, err, sizeof(err)),
              CBM_SESSION_OK);
    ASSERT_NULL(cbm_session_find_bound_context(&fx.sessions, &context));
    session_fixture_cleanup(&fx);
    PASS();
}

TEST(test_mutation_journal_is_durable_and_outcomes_are_idempotent) {
    SessionFixture fx;
    if (session_fixture_init(&fx) != 0) FAIL("fixture init");

    char journal_path[256];
    snprintf(journal_path, sizeof(journal_path), "%s/mutation.sqlite", fx.dir);
    CbmMutationJournal journal = {0};
    ASSERT_EQ(cbm_mutation_journal_open(&journal, journal_path), CBM_MUTATION_JOURNAL_OK);

    CbmMutationAttempt attempt = {0};
    attempt.host_context.host = CBM_MUTATION_HOST_ANTIGRAVITY;
    snprintf(attempt.host_context.context_id, sizeof(attempt.host_context.context_id), "conversation_7");
    attempt.operation = CBM_MUTATION_OPERATION_MODIFY;
    attempt.scope = CBM_MUTATION_SCOPE_REPOSITORY;
    snprintf(attempt.attempt_id, sizeof(attempt.attempt_id), "attempt_1");
    CbmChangeGrounding grounding = {0};
    grounding.kind = CBM_GROUNDING_CANON_CITATION;
    snprintf(grounding.intent_key, sizeof(grounding.intent_key), "edit_parser");
    snprintf(grounding.reference, sizeof(grounding.reference), "@inst/architecture#parser");
    ASSERT_TRUE(session_test_set_target(&fx, &attempt, &grounding,
                                        CBM_MUTATION_OPERATION_MODIFY, "parser.c"));

    ASSERT_EQ(cbm_mutation_journal_register_session(&journal, "horizon_1",
                                                    &attempt.host_context, &grounding),
              CBM_MUTATION_JOURNAL_OK);
    ASSERT_EQ(cbm_mutation_journal_register_session(&journal, "horizon_1",
                                                    &attempt.host_context, &grounding),
              CBM_MUTATION_JOURNAL_OK);
    ASSERT_EQ(cbm_mutation_journal_register_session(&journal, "horizon_2",
                                                    &attempt.host_context, &grounding),
              CBM_MUTATION_JOURNAL_ERR_STORAGE);

    char write_id[CBM_MUTATION_WRITE_ID_MAX] = {0};
    ASSERT_EQ(cbm_mutation_journal_commit_intent(&journal, "horizon_1", &attempt,
                                                 &grounding, write_id, sizeof(write_id)),
              CBM_MUTATION_JOURNAL_OK);
    ASSERT_TRUE(write_id[0] != '\0');
    CbmMutationOutcome outcome = CBM_MUTATION_OUTCOME_NONE;
    ASSERT_EQ(cbm_mutation_journal_get_outcome(&journal, write_id, &outcome),
              CBM_MUTATION_JOURNAL_OK);
    ASSERT_EQ(outcome, CBM_MUTATION_OUTCOME_PENDING);

    ASSERT_EQ(cbm_mutation_journal_observe(&journal, write_id,
                                           CBM_MUTATION_OUTCOME_OBSERVED_APPLIED),
              CBM_MUTATION_JOURNAL_OK);
    ASSERT_EQ(cbm_mutation_journal_observe(&journal, write_id,
                                           CBM_MUTATION_OUTCOME_OBSERVED_APPLIED),
              CBM_MUTATION_JOURNAL_OK);
    ASSERT_EQ(cbm_mutation_journal_observe(&journal, write_id,
                                           CBM_MUTATION_OUTCOME_OBSERVED_FAILED),
              CBM_MUTATION_JOURNAL_ERR_OUTCOME_CONFLICT);

    snprintf(attempt.attempt_id, sizeof(attempt.attempt_id), "attempt_2");
    char unresolved_id[CBM_MUTATION_WRITE_ID_MAX] = {0};
    ASSERT_EQ(cbm_mutation_journal_commit_intent(&journal, "horizon_1", &attempt,
                                                 &grounding, unresolved_id,
                                                 sizeof(unresolved_id)),
              CBM_MUTATION_JOURNAL_OK);
    cbm_mutation_journal_close(&journal);

    ASSERT_EQ(cbm_mutation_journal_open(&journal, journal_path), CBM_MUTATION_JOURNAL_OK);
    ASSERT_EQ(cbm_mutation_journal_recover_pending(&journal), CBM_MUTATION_JOURNAL_OK);
    ASSERT_EQ(cbm_mutation_journal_get_outcome(&journal, unresolved_id, &outcome),
              CBM_MUTATION_JOURNAL_OK);
    ASSERT_EQ(outcome, CBM_MUTATION_OUTCOME_UNKNOWN);
    CbmMutationSessionBinding binding = {0};
    bool other_context = false;
    ASSERT_EQ(cbm_mutation_journal_find_session(&journal, &attempt.host_context,
                                                &binding, &other_context),
              CBM_MUTATION_JOURNAL_ERR_NOT_FOUND);
    ASSERT_FALSE(other_context);
    cbm_mutation_journal_close(&journal);
    session_fixture_cleanup(&fx);
    PASS();
}

TEST(test_mutation_authorization_requires_session_and_committed_intent) {
    SessionFixture fx;
    if (session_fixture_init(&fx) != 0) FAIL("fixture init");
    CbmSessionHorizon session;
    char err[CBM_CONTRACT_ERROR_MAX] = {0};
    ASSERT_EQ(cbm_session_open(&fx.sessions, &fx.contracts, &fx.pool, 4242,
                              "graph_grounding", "graph_grounding", "seq_41",
                              &session, err, sizeof(err)), CBM_SESSION_OK);

    CbmMutationAttempt attempt = {0};
    attempt.host_context.host = CBM_MUTATION_HOST_CODEX;
    snprintf(attempt.host_context.context_id, sizeof(attempt.host_context.context_id), "thread_42");
    attempt.operation = CBM_MUTATION_OPERATION_MODIFY;
    attempt.scope = CBM_MUTATION_SCOPE_REPOSITORY;
    snprintf(attempt.attempt_id, sizeof(attempt.attempt_id), "tool_call_9");
    CbmChangeGrounding grounding = {0};
    grounding.kind = CBM_GROUNDING_CANON_CITATION;
    snprintf(grounding.intent_key, sizeof(grounding.intent_key), "change_42");
    snprintf(grounding.reference, sizeof(grounding.reference), "@inst/architecture#rule-3");
    ASSERT_TRUE(session_test_set_target(&fx, &attempt, &grounding,
                                        CBM_MUTATION_OPERATION_MODIFY, "rule.c"));

    char journal_path[256];
    snprintf(journal_path, sizeof(journal_path), "%s/authorization.sqlite", fx.dir);
    CbmMutationJournal journal = {0};
    ASSERT_EQ(cbm_mutation_journal_open(&journal, journal_path), CBM_MUTATION_JOURNAL_OK);

    CbmMutationDecision decision = cbm_mutation_authorize_repository_write(&journal, &attempt);
    ASSERT_FALSE(decision.permitted);
    ASSERT_EQ(decision.refusal.code, CBM_MUTATION_REFUSAL_SESSION_UNBOUND);

    ASSERT_EQ(cbm_session_bind_mutation_context(&fx.sessions, session.horizon_id,
                                                &attempt.host_context, &grounding,
                                                err, sizeof(err)), CBM_SESSION_OK);

    /* A hook is a distinct short-lived process with a distinct in-memory
     * registry. It must observe only durable session authority. */
    ASSERT_EQ(cbm_mutation_journal_register_session(&journal, session.horizon_id,
                                                   &attempt.host_context, &grounding),
              CBM_MUTATION_JOURNAL_OK);
    CbmMutationJournal hook_journal = {0};
    ASSERT_EQ(cbm_mutation_journal_open(&hook_journal, journal_path),
              CBM_MUTATION_JOURNAL_OK);
    decision = cbm_mutation_authorize_repository_write(&hook_journal, &attempt);
    ASSERT_TRUE(decision.permitted);
    ASSERT_TRUE(decision.write_id[0] != '\0');
    CbmMutationOutcome outcome = CBM_MUTATION_OUTCOME_NONE;
    ASSERT_EQ(cbm_mutation_journal_get_outcome(&hook_journal, decision.write_id, &outcome),
              CBM_MUTATION_JOURNAL_OK);
    ASSERT_EQ(outcome, CBM_MUTATION_OUTCOME_PENDING);

    char original_target[CBM_MUTATION_TARGET_PATH_MAX];
    snprintf(original_target, sizeof(original_target), "%s", attempt.target_path);
    char other_requested[CBM_MUTATION_CANONICAL_PATH_MAX];
    int other_written = snprintf(other_requested, sizeof(other_requested), "%s/other.c", fx.dir);
    ASSERT_TRUE(other_written > 0 && (size_t)other_written < sizeof(other_requested));
    ASSERT_TRUE(cbm_mutation_canonicalize_path(other_requested, attempt.target_path,
                                               sizeof(attempt.target_path)));
    decision = cbm_mutation_authorize_repository_write(&hook_journal, &attempt);
    ASSERT_FALSE(decision.permitted);
    ASSERT_EQ(decision.refusal.code, CBM_MUTATION_REFUSAL_INTENT_SCOPE_MISMATCH);
    snprintf(attempt.target_path, sizeof(attempt.target_path), "%s", original_target);
    attempt.operation = CBM_MUTATION_OPERATION_CREATE;
    decision = cbm_mutation_authorize_repository_write(&hook_journal, &attempt);
    ASSERT_FALSE(decision.permitted);
    ASSERT_EQ(decision.refusal.code, CBM_MUTATION_REFUSAL_INTENT_SCOPE_MISMATCH);
    attempt.operation = CBM_MUTATION_OPERATION_MODIFY;

    snprintf(attempt.host_context.context_id, sizeof(attempt.host_context.context_id), "thread_99");
    decision = cbm_mutation_authorize_repository_write(&hook_journal, &attempt);
    ASSERT_FALSE(decision.permitted);
    ASSERT_EQ(decision.refusal.code, CBM_MUTATION_REFUSAL_SESSION_STALE);
    ASSERT_EQ(cbm_mutation_journal_mark_session_unknown(&journal, session.horizon_id),
              CBM_MUTATION_JOURNAL_OK);
    snprintf(attempt.host_context.context_id, sizeof(attempt.host_context.context_id), "thread_42");
    decision = cbm_mutation_authorize_repository_write(&hook_journal, &attempt);
    ASSERT_FALSE(decision.permitted);
    ASSERT_EQ(decision.refusal.code, CBM_MUTATION_REFUSAL_SESSION_UNBOUND);
    cbm_mutation_journal_close(&hook_journal);
    cbm_mutation_journal_close(&journal);
    session_fixture_cleanup(&fx);
    PASS();
}

SUITE(union_session) {
    RUN_TEST(test_session_open_registered);
    RUN_TEST(test_session_open_unregistered_restricted);
    RUN_TEST(test_session_based_on_seq_recorded);
    RUN_TEST(test_session_counters_reach_closure);
    RUN_TEST(test_session_close_destroys_content);
    RUN_TEST(test_session_empty_close_first_class);
    RUN_TEST(test_session_registry_full);
    RUN_TEST(test_mutation_effect_classification);
    RUN_TEST(test_mutation_host_context_requires_stable_identity);
    RUN_TEST(test_change_grounding_requires_intent_and_provenance);
    RUN_TEST(test_mutation_rename_authorizes_and_journals_both_endpoints);
    RUN_TEST(test_session_mutation_context_is_bound_and_not_replayable);
    RUN_TEST(test_mutation_journal_is_durable_and_outcomes_are_idempotent);
    RUN_TEST(test_mutation_authorization_requires_session_and_committed_intent);
}
