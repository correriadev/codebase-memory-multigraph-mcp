/*
 * union_contract.h — Skill Contract Registry (Scope A02).
 *
 * A skill enters the union only by declaring its archetypal boundary
 * machine-readably: identity, territory, effect class, obligations, and the
 * refusal matrix it acknowledges. A skill without a registered contract is
 * not refused outright — it operates in restricted mode (every action gated
 * at the irreversible class). Territory overlap with an existing contract is
 * reported, never silently merged.
 */
#ifndef CBM_UNION_CONTRACT_H
#define CBM_UNION_CONTRACT_H

#include "union_refusal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CBM_CONTRACT_ID_MAX 64
#define CBM_CONTRACT_TERRITORY_MAX 8
#define CBM_CONTRACT_TERRITORY_MAX_LEN 48
#define CBM_CONTRACT_PROVENANCE_MAX 128
#define CBM_CONTRACT_REGISTRY_CAP 64
#define CBM_CONTRACT_ERROR_MAX 128

typedef enum {
    CBM_EFFECT_IDEMPOTENT = 0,
    CBM_EFFECT_COMPENSABLE = 1,
    CBM_EFFECT_IRREVERSIBLE = 2,
    /* Sentinel for an unclassified action: treated as irreversible and
     * blocked without authorization (SCOPE-A05). A contract declaring this
     * is refused at validation — the contract must classify everything it
     * declares. */
    CBM_EFFECT_UNCLASSIFIED = 3
} CbmEffectClass;

const char *cbm_effect_class_string(CbmEffectClass cls);

typedef struct {
    char identity[CBM_CONTRACT_ID_MAX];

    /* Claim types / node labels this archetype may author. Territory is
     * non-overlapping by contract: overlaps are reported, not merged. */
    char territory[CBM_CONTRACT_TERRITORY_MAX][CBM_CONTRACT_TERRITORY_MAX_LEN];
    size_t territory_count;

    /* Default effect class for the contract's actions (SCOPE-A05). */
    CbmEffectClass effect_class;

    /* Bitmask over CbmRefusalCode: the codes this skill acknowledges with a
     * mandated behavior (SCOPE-B04). Bit i set => code i acknowledged. */
    uint32_t acknowledged_refusals_mask;

    /* Obligations (SCOPE-A07/A09): trace on closure, exclusion summary on
     * promotion. */
    bool requires_trace;
    bool requires_exclusion_summary;

    /* Lineage of the skill's own evolution (SCOPE-A02 open question 1). */
    char provenance[CBM_CONTRACT_PROVENANCE_MAX];
} CbmSkillContract;

typedef enum {
    CBM_CONTRACT_OK = 0,
    CBM_CONTRACT_ERR_NULL = -1,
    CBM_CONTRACT_ERR_INVALID = -2,   /* validation failed; error names the field */
    CBM_CONTRACT_ERR_FULL = -3,      /* registry capacity exhausted */
    CBM_CONTRACT_ERR_EXISTS = -4     /* identity already registered */
} CbmContractResult;

typedef struct {
    CbmSkillContract contracts[CBM_CONTRACT_REGISTRY_CAP];
    size_t count;
} CbmContractRegistry;

void cbm_contract_registry_init(CbmContractRegistry *registry);

/* Validate a contract. On failure writes the exact missing/invalid field
 * name into out_error (CONTRACT_INVALID names the field — SCOPE-A02 AC1). */
CbmContractResult cbm_contract_validate(const CbmSkillContract *contract,
                                       char *out_error, size_t err_sz);

/* Submit a contract to the registry. Validates first; a duplicate identity
 * is refused with CBM_CONTRACT_ERR_EXISTS (supersession is a later scope —
 * the registry never edits a registered contract). */
CbmContractResult cbm_contract_registry_put(CbmContractRegistry *registry,
                                            const CbmSkillContract *contract,
                                            char *out_error, size_t err_sz);

/* Look up a contract by identity. NULL when unknown (the caller applies
 * restricted mode — the registry refuses nothing on its own here). */
const CbmSkillContract *cbm_contract_registry_get(const CbmContractRegistry *registry,
                                                  const char *identity);

/* Territory-overlap report (SCOPE-A02 AC4): fill out_ids with the identities
 * of registered contracts sharing at least one territory string with the
 * candidate. Returns the overlap count (may exceed max_out — only max_out
 * ids are written). */
size_t cbm_contract_registry_overlap(const CbmContractRegistry *registry,
                                     const CbmSkillContract *candidate,
                                     char out_ids[][CBM_CONTRACT_ID_MAX],
                                     size_t max_out);

#endif /* CBM_UNION_CONTRACT_H */
