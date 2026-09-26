/*
 * test_union_session.c — Scope A01 acceptance criteria.
 *
 * Uses a real HorizonConnectionPool on a temp dir: the protocol wraps the
 * existing horizon machinery, it does not mock it.
 */
#include "test_framework.h"
#include "test_helpers.h"
#include "../src/union/union_session.h"
#include "../src/foundation/compat.h"

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

SUITE(union_session) {
    RUN_TEST(test_session_open_registered);
    RUN_TEST(test_session_open_unregistered_restricted);
    RUN_TEST(test_session_based_on_seq_recorded);
    RUN_TEST(test_session_counters_reach_closure);
    RUN_TEST(test_session_close_destroys_content);
    RUN_TEST(test_session_empty_close_first_class);
    RUN_TEST(test_session_registry_full);
}
