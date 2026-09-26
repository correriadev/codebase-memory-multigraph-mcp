/*
 * union_ledger.c — Horizon Budget Ledger (Scope A06).
 */
#include "union_ledger.h"

#include <stdio.h>
#include <string.h>

void cbm_ledger_init(CbmHorizonLedger *ledger, const char *horizon_id, const CbmHorizonBudgetLimits *limits) {
    if (!ledger) return;
    memset(ledger, 0, sizeof(*ledger));
    if (horizon_id) {
        snprintf(ledger->horizon_id, sizeof(ledger->horizon_id), "%s", horizon_id);
    }
    if (limits) {
        ledger->limits = *limits;
    }
}

CbmRefusalCode cbm_ledger_debit_attempt(CbmHorizonLedger *ledger) {
    if (!ledger) return CBM_REFUSAL_CONTRACT_INVALID;
    if (ledger->exhausted) return CBM_REFUSAL_BUDGET_EXHAUSTED;

    if (ledger->limits.max_attempts > 0 && ledger->used_attempts >= ledger->limits.max_attempts) {
        ledger->exhausted = true;
        ledger->exhausted_dimension = CBM_REFUSAL_BUDGET_EXHAUSTED;
        cbm_refusal_emit(CBM_REFUSAL_BUDGET_EXHAUSTED, ledger->horizon_id, "attempt_budget_exhausted");
        return CBM_REFUSAL_BUDGET_EXHAUSTED;
    }

    ledger->used_attempts++;
    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_ledger_debit_tokens(CbmHorizonLedger *ledger, uint64_t tokens) {
    if (!ledger) return CBM_REFUSAL_CONTRACT_INVALID;
    if (ledger->exhausted) return CBM_REFUSAL_BUDGET_EXHAUSTED;

    if (ledger->limits.max_tokens > 0 && (ledger->used_tokens + tokens) > ledger->limits.max_tokens) {
        ledger->exhausted = true;
        ledger->exhausted_dimension = CBM_REFUSAL_BUDGET_EXHAUSTED;
        cbm_refusal_emit(CBM_REFUSAL_BUDGET_EXHAUSTED, ledger->horizon_id, "token_budget_exhausted");
        return CBM_REFUSAL_BUDGET_EXHAUSTED;
    }

    ledger->used_tokens += tokens;
    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_ledger_debit_time(CbmHorizonLedger *ledger, uint64_t seconds) {
    if (!ledger) return CBM_REFUSAL_CONTRACT_INVALID;
    if (ledger->exhausted) return CBM_REFUSAL_BUDGET_EXHAUSTED;

    if (ledger->limits.max_time_sec > 0 && (ledger->used_time_sec + seconds) > ledger->limits.max_time_sec) {
        ledger->exhausted = true;
        ledger->exhausted_dimension = CBM_REFUSAL_BUDGET_EXHAUSTED;
        cbm_refusal_emit(CBM_REFUSAL_BUDGET_EXHAUSTED, ledger->horizon_id, "time_budget_exhausted");
        return CBM_REFUSAL_BUDGET_EXHAUSTED;
    }

    ledger->used_time_sec += seconds;
    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_ledger_debit_action(CbmHorizonLedger *ledger, CbmEffectClass effect_class) {
    if (!ledger) return CBM_REFUSAL_CONTRACT_INVALID;
    if (ledger->exhausted) return CBM_REFUSAL_BUDGET_EXHAUSTED;

    if (effect_class == CBM_EFFECT_IDEMPOTENT) {
        ledger->actions_idempotent++;
    } else if (effect_class == CBM_EFFECT_COMPENSABLE) {
        ledger->actions_compensable++;
    } else if (effect_class == CBM_EFFECT_IRREVERSIBLE) {
        if (ledger->limits.max_irreversible_actions > 0 &&
            ledger->actions_irreversible >= ledger->limits.max_irreversible_actions) {
            ledger->exhausted = true;
            ledger->exhausted_dimension = CBM_REFUSAL_BUDGET_EXHAUSTED;
            cbm_refusal_emit(CBM_REFUSAL_BUDGET_EXHAUSTED, ledger->horizon_id, "irreversible_actions_exhausted");
            return CBM_REFUSAL_BUDGET_EXHAUSTED;
        }
        ledger->actions_irreversible++;
    }

    return CBM_REFUSAL_OK;
}

bool cbm_ledger_raise_budget(CbmHorizonLedger *ledger,
                            const CbmHorizonBudgetLimits *new_limits,
                            const char *operator_id,
                            const char *provenance) {
    if (!ledger || !new_limits || !operator_id || !operator_id[0]) return false;

    ledger->limits = *new_limits;
    ledger->exhausted = false;
    ledger->escalation_count++;
    snprintf(ledger->last_escalation_operator, sizeof(ledger->last_escalation_operator), "%s", operator_id);
    if (provenance) {
        snprintf(ledger->last_escalation_provenance, sizeof(ledger->last_escalation_provenance), "%s", provenance);
    } else {
        ledger->last_escalation_provenance[0] = '\0';
    }

    fprintf(stderr, "level=info msg=union.ledger.escalate horizon_id=%s operator=%s count=%u\n",
            ledger->horizon_id, operator_id, ledger->escalation_count);
    return true;
}

bool cbm_ledger_can_promote(const CbmHorizonLedger *ledger) {
    if (!ledger) return false;
    /* Invariant FR-14: exhaustion never promotes */
    return !ledger->exhausted;
}
