/*
 * union_ledger.h — Horizon Budget Ledger (Scope A06).
 *
 * Tracks the two currencies: time and memory.
 * Debits per action across dimensions: attempts, tokens, duration, and effect class counts.
 * When exhausted, emits BUDGET_EXHAUSTED and blocks progression.
 * Raising a budget requires explicit operator escalation with provenance.
 */
#ifndef CBM_UNION_LEDGER_H
#define CBM_UNION_LEDGER_H

#include "union_contract.h"
#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_LEDGER_HORIZON_ID_MAX 64
#define CBM_LEDGER_OPERATOR_MAX 64
#define CBM_LEDGER_PROVENANCE_MAX 128

typedef struct {
    uint32_t max_attempts;
    uint64_t max_time_sec;
    uint64_t max_tokens;
    uint32_t max_irreversible_actions;
} CbmHorizonBudgetLimits;

typedef struct {
    char horizon_id[CBM_LEDGER_HORIZON_ID_MAX];
    CbmHorizonBudgetLimits limits;
    uint32_t used_attempts;
    uint64_t used_time_sec;
    uint64_t used_tokens;
    uint32_t actions_idempotent;
    uint32_t actions_compensable;
    uint32_t actions_irreversible;
    bool exhausted;
    CbmRefusalCode exhausted_dimension;
    uint32_t escalation_count;
    char last_escalation_operator[CBM_LEDGER_OPERATOR_MAX];
    char last_escalation_provenance[CBM_LEDGER_PROVENANCE_MAX];
} CbmHorizonLedger;

void cbm_ledger_init(CbmHorizonLedger *ledger, const char *horizon_id, const CbmHorizonBudgetLimits *limits);

/* Debit methods return CBM_REFUSAL_OK on success or CBM_REFUSAL_BUDGET_EXHAUSTED if limit reached. */
CbmRefusalCode cbm_ledger_debit_attempt(CbmHorizonLedger *ledger);
CbmRefusalCode cbm_ledger_debit_tokens(CbmHorizonLedger *ledger, uint64_t tokens);
CbmRefusalCode cbm_ledger_debit_time(CbmHorizonLedger *ledger, uint64_t seconds);
CbmRefusalCode cbm_ledger_debit_action(CbmHorizonLedger *ledger, CbmEffectClass effect_class);

/* Raise budget: explicit operator escalation with recorded provenance. Resumes exhausted state. */
bool cbm_ledger_raise_budget(CbmHorizonLedger *ledger,
                            const CbmHorizonBudgetLimits *new_limits,
                            const char *operator_id,
                            const char *provenance);

/* Reachability check for AC3: returns false if ledger is exhausted (exhaustion never promotes). */
bool cbm_ledger_can_promote(const CbmHorizonLedger *ledger);

#endif /* CBM_UNION_LEDGER_H */
