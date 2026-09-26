/*
 * union_gateway.h — Effect-Class Gateway (Scope A05).
 *
 * Enforces action consequence classification:
 * - IDEMPOTENT: repeatable, no side effects.
 * - COMPENSABLE: idempotency key + compensation recording.
 * - IRREVERSIBLE: registration precedes execution + scoped operator authorization,
 *   single-use, expiring, snapshot-bound.
 * - UNCLASSIFIED: treated as irreversible and blocked absent authorization (TOOL_UNCLASSIFIED).
 */
#ifndef CBM_UNION_GATEWAY_H
#define CBM_UNION_GATEWAY_H

#include "union_contract.h"
#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_GATEWAY_AUTH_ID_MAX 64
#define CBM_GATEWAY_SCOPE_MAX 64
#define CBM_GATEWAY_ACTION_NAME_MAX 64
#define CBM_GATEWAY_IDEMPOTENCY_KEY_MAX 64
#define CBM_GATEWAY_AUTH_REGISTRY_CAP 64
#define CBM_GATEWAY_COMPENSABLE_CAP 64
#define CBM_GATEWAY_SEQ_MAX 64

typedef struct {
    char auth_id[CBM_GATEWAY_AUTH_ID_MAX];
    char scope[CBM_GATEWAY_SCOPE_MAX];
    char target_action[CBM_GATEWAY_ACTION_NAME_MAX];
    char snapshot_seq[CBM_GATEWAY_SEQ_MAX];
    uint64_t valid_until_unix;
    bool used;
} CbmScopedAuthorization;

typedef struct {
    char idempotency_key[CBM_GATEWAY_IDEMPOTENCY_KEY_MAX];
    char action_name[CBM_GATEWAY_ACTION_NAME_MAX];
    bool executed;
    bool compensated;
    uint64_t executed_at;
} CbmCompensableRecord;

typedef struct {
    CbmScopedAuthorization authorizations[CBM_GATEWAY_AUTH_REGISTRY_CAP];
    size_t auth_count;
    CbmCompensableRecord compensable[CBM_GATEWAY_COMPENSABLE_CAP];
    size_t compensable_count;
} CbmGateway;

typedef enum {
    CBM_GATEWAY_ALLOW = 0,
    CBM_GATEWAY_BLOCK = -1
} CbmGatewayDecision;

void cbm_gateway_init(CbmGateway *gateway);

/* Register a scoped operator authorization for an irreversible action. */
bool cbm_gateway_register_authorization(CbmGateway *gateway, const CbmScopedAuthorization *auth);

/* Authorize an action through the gateway.
 * For irreversible actions, checks authorization existence, single-use, scope, and TTL.
 * Emits intent to log before effect. */
CbmGatewayDecision cbm_gateway_authorize_action(CbmGateway *gateway,
                                               const char *horizon_id,
                                               const char *skill_identity,
                                               const CbmSkillContract *contract,
                                               const char *action_name,
                                               CbmEffectClass effect_class,
                                               const char *auth_id,
                                               const char *current_seq,
                                               uint64_t current_time_unix,
                                               const char *idempotency_key,
                                               CbmRefusalCode *out_refusal,
                                               char *out_reason, size_t reason_sz);

/* Record compensation for a compensable action. */
bool cbm_gateway_record_compensation(CbmGateway *gateway, const char *horizon_id, const char *idempotency_key);

#endif /* CBM_UNION_GATEWAY_H */
