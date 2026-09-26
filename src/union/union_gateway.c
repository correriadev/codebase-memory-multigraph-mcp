/*
 * union_gateway.c — Effect-Class Gateway (Scope A05).
 */
#include "union_gateway.h"

#include <stdio.h>
#include <string.h>

void cbm_gateway_init(CbmGateway *gateway) {
    if (!gateway) return;
    memset(gateway, 0, sizeof(*gateway));
}

bool cbm_gateway_register_authorization(CbmGateway *gateway, const CbmScopedAuthorization *auth) {
    if (!gateway || !auth || !auth->auth_id[0]) return false;
    if (gateway->auth_count >= CBM_GATEWAY_AUTH_REGISTRY_CAP) return false;

    for (size_t i = 0; i < gateway->auth_count; i++) {
        if (strcmp(gateway->authorizations[i].auth_id, auth->auth_id) == 0) {
            return false; /* already registered */
        }
    }

    gateway->authorizations[gateway->auth_count] = *auth;
    gateway->authorizations[gateway->auth_count].used = false;
    gateway->auth_count++;
    return true;
}

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
                                               char *out_reason, size_t reason_sz) {
    if (out_refusal) *out_refusal = CBM_REFUSAL_OK;
    if (out_reason && reason_sz > 0) out_reason[0] = '\0';

    if (!gateway || !action_name) {
        if (out_refusal) *out_refusal = CBM_REFUSAL_CONTRACT_INVALID;
        return CBM_GATEWAY_BLOCK;
    }

    /* 1. Unclassified action: treated as irreversible and blocked absent authorization */
    if (effect_class == CBM_EFFECT_UNCLASSIFIED) {
        if (out_refusal) *out_refusal = CBM_REFUSAL_TOOL_UNCLASSIFIED;
        if (out_reason && reason_sz > 0) {
            snprintf(out_reason, reason_sz, "unclassified_action:%s", action_name);
        }
        cbm_refusal_emit(CBM_REFUSAL_TOOL_UNCLASSIFIED, horizon_id, "action_unclassified");
        return CBM_GATEWAY_BLOCK;
    }

    /* 2. Idempotent action */
    if (effect_class == CBM_EFFECT_IDEMPOTENT) {
        fprintf(stderr, "level=info msg=union.gateway.allow horizon_id=%s action=%s class=IDEMPOTENT\n",
                horizon_id ? horizon_id : "", action_name);
        return CBM_GATEWAY_ALLOW;
    }

    /* 3. Compensable action */
    if (effect_class == CBM_EFFECT_COMPENSABLE) {
        if (idempotency_key && idempotency_key[0]) {
            for (size_t i = 0; i < gateway->compensable_count; i++) {
                if (strcmp(gateway->compensable[i].idempotency_key, idempotency_key) == 0) {
                    fprintf(stderr, "level=info msg=union.gateway.replay horizon_id=%s action=%s key=%s\n",
                            horizon_id ? horizon_id : "", action_name, idempotency_key);
                    return CBM_GATEWAY_ALLOW;
                }
            }
            if (gateway->compensable_count < CBM_GATEWAY_COMPENSABLE_CAP) {
                CbmCompensableRecord *rec = &gateway->compensable[gateway->compensable_count++];
                snprintf(rec->idempotency_key, sizeof(rec->idempotency_key), "%s", idempotency_key);
                snprintf(rec->action_name, sizeof(rec->action_name), "%s", action_name);
                rec->executed = true;
                rec->compensated = false;
                rec->executed_at = current_time_unix;
            }
        }
        fprintf(stderr, "level=info msg=union.gateway.allow horizon_id=%s action=%s class=COMPENSABLE\n",
                horizon_id ? horizon_id : "", action_name);
        return CBM_GATEWAY_ALLOW;
    }

    /* 4. Irreversible action: requires scoped authorization */
    if (effect_class == CBM_EFFECT_IRREVERSIBLE) {
        if (!auth_id || !auth_id[0]) {
            if (out_refusal) *out_refusal = CBM_REFUSAL_SCOPE_EXCEEDED;
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz, "authorization_missing_for_irreversible_action:%s", action_name);
            }
            cbm_refusal_emit(CBM_REFUSAL_SCOPE_EXCEEDED, horizon_id, "authorization_missing");
            return CBM_GATEWAY_BLOCK;
        }

        CbmScopedAuthorization *found = NULL;
        for (size_t i = 0; i < gateway->auth_count; i++) {
            if (strcmp(gateway->authorizations[i].auth_id, auth_id) == 0) {
                found = &gateway->authorizations[i];
                break;
            }
        }

        if (!found) {
            if (out_refusal) *out_refusal = CBM_REFUSAL_SCOPE_EXCEEDED;
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz, "authorization_not_found:%s", auth_id);
            }
            cbm_refusal_emit(CBM_REFUSAL_SCOPE_EXCEEDED, horizon_id, "authorization_not_found");
            return CBM_GATEWAY_BLOCK;
        }

        /* Check single-use */
        if (found->used) {
            if (out_refusal) *out_refusal = CBM_REFUSAL_RETRY_IDENTICAL;
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz, "authorization_replayed:%s", auth_id);
            }
            cbm_refusal_emit(CBM_REFUSAL_RETRY_IDENTICAL, horizon_id, "authorization_replayed");
            return CBM_GATEWAY_BLOCK;
        }

        /* Check TTL */
        if (found->valid_until_unix > 0 && current_time_unix > found->valid_until_unix) {
            if (out_refusal) *out_refusal = CBM_REFUSAL_SCOPE_EXCEEDED;
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz, "authorization_expired:%s", auth_id);
            }
            cbm_refusal_emit(CBM_REFUSAL_SCOPE_EXCEEDED, horizon_id, "authorization_expired");
            return CBM_GATEWAY_BLOCK;
        }

        /* Check action target */
        if (found->target_action[0] && strcmp(found->target_action, action_name) != 0) {
            if (out_refusal) *out_refusal = CBM_REFUSAL_SCOPE_EXCEEDED;
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz, "authorization_action_mismatch:%s!=%s",
                         found->target_action, action_name);
            }
            cbm_refusal_emit(CBM_REFUSAL_SCOPE_EXCEEDED, horizon_id, "action_mismatch");
            return CBM_GATEWAY_BLOCK;
        }

        /* Check snapshot staleness */
        if (found->snapshot_seq[0] && current_seq && current_seq[0] &&
            strcmp(found->snapshot_seq, current_seq) != 0) {
            if (out_refusal) *out_refusal = CBM_REFUSAL_STALE_BASE;
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz, "authorization_snapshot_stale:%s!=%s",
                         found->snapshot_seq, current_seq);
            }
            cbm_refusal_emit(CBM_REFUSAL_STALE_BASE, horizon_id, "authorization_snapshot_stale");
            return CBM_GATEWAY_BLOCK;
        }

        /* Invariant AC2: Log intent strictly before effect */
        fprintf(stderr, "level=info msg=union.gateway.intent horizon_id=%s action=%s auth_id=%s\n",
                horizon_id ? horizon_id : "", action_name, auth_id);

        /* Consume authorization (single-use) */
        found->used = true;
        return CBM_GATEWAY_ALLOW;
    }

    return CBM_GATEWAY_BLOCK;
}

bool cbm_gateway_record_compensation(CbmGateway *gateway, const char *horizon_id, const char *idempotency_key) {
    if (!gateway || !idempotency_key) return false;
    for (size_t i = 0; i < gateway->compensable_count; i++) {
        if (strcmp(gateway->compensable[i].idempotency_key, idempotency_key) == 0) {
            gateway->compensable[i].compensated = true;
            fprintf(stderr, "level=info msg=union.gateway.compensation horizon_id=%s key=%s\n",
                    horizon_id ? horizon_id : "", idempotency_key);
            return true;
        }
    }
    return false;
}
