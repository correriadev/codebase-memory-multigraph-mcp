/*
 * union_session.h — Session Horizon Protocol (Scope A01).
 *
 * Every skill invocation is a bounded temenos: a session horizon with
 * identity, the belief-sequence it is based on, a lifecycle the host can
 * audit, and a typed closure event. Content is destroyed at closure; events
 * survive. An identity without a registered contract opens in RESTRICTED
 * MODE — restricted, not refused (every action gated at the irreversible
 * class; enforcement of the gate itself is SCOPE-A05).
 */
#ifndef CBM_UNION_SESSION_H
#define CBM_UNION_SESSION_H

#include "union_contract.h"
#include "union_refusal.h"

#include "../core/horizon_pool.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_SESSION_IDENTITY_MAX 64
#define CBM_SESSION_BASED_ON_SEQ_MAX 64
#define CBM_SESSION_REASON_MAX 64
#define CBM_SESSION_REGISTRY_CAP 32

typedef enum {
    CBM_SESSION_OK = 0,
    CBM_SESSION_ERR_NULL = -1,
    CBM_SESSION_ERR_POOL = -2,       /* horizon creation failed */
    CBM_SESSION_ERR_FULL = -3,       /* session registry exhausted */
    CBM_SESSION_ERR_NOT_FOUND = -4   /* close/query on unknown session */
} CbmSessionResult;

typedef enum {
    CBM_SESSION_CLOSE_NORMAL = 0,
    CBM_SESSION_CLOSE_ABNORMAL = 1,  /* crash path: what already happened survives */
    CBM_SESSION_CLOSE_EMPTY = 2      /* first-class: nothing done, honestly */
} CbmSessionCloseReason;

const char *cbm_session_close_reason_string(CbmSessionCloseReason reason);

typedef struct {
    char horizon_id[CBM_HORIZON_ID_MAX];
    char identity[CBM_SESSION_IDENTITY_MAX];
    char contract_id[CBM_CONTRACT_ID_MAX];
    bool restricted; /* no registered contract: irreversible-class gating */
    CbmRefusalCode open_refusal; /* CBM_REFUSAL_OK, or CONTRACT_UNKNOWN when restricted */
    char based_on_seq[CBM_SESSION_BASED_ON_SEQ_MAX];
    uint64_t opened_at_unix;
    uint32_t actions;
    uint32_t refusals;
} CbmSessionHorizon;

typedef struct {
    char horizon_id[CBM_HORIZON_ID_MAX];
    char identity[CBM_SESSION_IDENTITY_MAX];
    char reason[CBM_SESSION_REASON_MAX];
    uint64_t duration_sec;
    uint32_t actions;
    uint32_t refusals;
} CbmSessionClosure;

typedef struct {
    CbmSessionHorizon sessions[CBM_SESSION_REGISTRY_CAP];
    size_t count;
} CbmSessionRegistry;

void cbm_session_registry_init(CbmSessionRegistry *registry);

/* Open a session horizon for a skill identity.
 *
 * - contract_id must be registered in contract_registry for full mode;
 *   an unknown/unregistered identity opens RESTRICTED (recorded, not
 *   refused) and open_refusal is CBM_REFUSAL_CONTRACT_UNKNOWN.
 * - based_on_seq is recorded verbatim and never silently re-based (A01 AC3);
 *   staleness detection is SCOPE-A04/A06, not this protocol.
 * - out_error (optional) carries the machine-readable refusal code string on
 *   failure. */
CbmSessionResult cbm_session_open(CbmSessionRegistry *sessions,
                                 CbmContractRegistry *contracts,
                                 HorizonConnectionPool *pool,
                                 uint32_t client_pid,
                                 const char *skill_identity,
                                 const char *contract_id,
                                 const char *based_on_seq,
                                 CbmSessionHorizon *out,
                                 char *out_error, size_t err_sz);

/* Query an open session's state by horizon id (A01 AC5: state is
 * reconstructible; this reads live state, the host log reconstructs). */
const CbmSessionHorizon *cbm_session_get(const CbmSessionRegistry *sessions,
                                         const char *horizon_id);

/* Record an action against the session ledger (counters only — the budget
 * ledger is SCOPE-A06). */
CbmSessionResult cbm_session_record_action(CbmSessionRegistry *sessions,
                                           const char *horizon_id);

/* Record a refusal received by the session. */
CbmSessionResult cbm_session_record_refusal(CbmSessionRegistry *sessions,
                                            const char *horizon_id);

/* Close a session horizon: emits the typed closure event (identity, reason,
 * duration, action/refusal counts), destroys the horizon content, and keeps
 * the event. out_closure is optional. An ABNORMAL close is a closure with
 * reason=ABNORMAL — a crash does not destroy what already happened. */
CbmSessionResult cbm_session_close(CbmSessionRegistry *sessions,
                                   HorizonConnectionPool *pool,
                                   const char *horizon_id,
                                   CbmSessionCloseReason reason,
                                   CbmSessionClosure *out_closure,
                                   char *out_error, size_t err_sz);

#endif /* CBM_UNION_SESSION_H */
