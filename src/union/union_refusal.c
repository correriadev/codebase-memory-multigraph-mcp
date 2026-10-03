/*
 * union_refusal.c — Typed Refusal Taxonomy (Scope A04).
 */
#include "union_refusal.h"

#include "../foundation/log.h"

#include <string.h>

typedef struct {
    CbmRefusalCode code;
    const char *name;
    const char *obligation;
} CbmRefusalDef;

/* Closed, versioned taxonomy. Order of entries matches the enum so the table
 * can never drift from the codes; adding a code supersedes, never edits. */
static const CbmRefusalDef k_refusal_defs[CBM_REFUSAL_CODE_COUNT] = {
    {CBM_REFUSAL_CONTRACT_INVALID, "CONTRACT_INVALID", "correct the named field; never re-submit the same contract"},
    {CBM_REFUSAL_CONTRACT_UNKNOWN, "CONTRACT_UNKNOWN", "register a contract before non-idempotent work; restricted mode applies until then"},
    {CBM_REFUSAL_TOOL_UNCLASSIFIED, "TOOL_UNCLASSIFIED", "declare the effect class; unclassified is treated irreversible and blocked"},
    {CBM_REFUSAL_ANCHOR_NOT_FOUND, "ANCHOR_NOT_FOUND", "re-ground or withdraw; never re-submit identical"},
    {CBM_REFUSAL_STALE_BASE, "STALE_BASE", "revalidate against current belief-sequence, or defer with operator visibility"},
    {CBM_REFUSAL_HORIZON_SKIP, "HORIZON_SKIP", "decompose to the proper scale; never widen the target"},
    {CBM_REFUSAL_ASSUMPTION_DROPPED, "ASSUMPTION_DROPPED", "restore or resolve the assumption with record"},
    {CBM_REFUSAL_EVIDENCE_REQUIRED, "EVIDENCE_REQUIRED", "produce evidence or concede; never argue"},
    {CBM_REFUSAL_BUDGET_EXHAUSTED, "BUDGET_EXHAUSTED", "escalate; never promote, never retry-loop"},
    {CBM_REFUSAL_EXCLUSION_UNDECLARED, "EXCLUSION_UNDECLARED", "declare the exclusion summary; silence is suppression"},
    {CBM_REFUSAL_PROVENANCE_MISSING, "PROVENANCE_MISSING", "return the item flagged, never as fact; park as open question"},
    {CBM_REFUSAL_CONTEST_UNPROVEN, "CONTEST_UNPROVEN", "withdraw or produce evidence; a contest without evidence does not exist"},
    {CBM_REFUSAL_SCOPE_EXCEEDED, "SCOPE_EXCEEDED", "obtain scoped authorization for this context; old consent does not transfer"},
    {CBM_REFUSAL_RETRY_IDENTICAL, "RETRY_IDENTICAL", "halt that line of work; identical re-submission is a discipline violation"},
    {CBM_REFUSAL_CLAIM_INVALID, "CLAIM_INVALID", "correct the named field; claims must satisfy structural and type constraints"},
    {CBM_REFUSAL_PREDICATE_NOT_SELF_CONTAINED, "PREDICATE_NOT_SELF_CONTAINED", "rephrase predicate as a self-contained single sentence without deictic references"},
    {CBM_REFUSAL_SWEEP_INCOMPLETE, "SWEEP_INCOMPLETE", "assign mandatory destinations to all proposed claims before closing session"},
    {CBM_REFUSAL_CODE_DOC_ASYMMETRY, "CODE_DOC_ASYMMETRY", "edges must be doc-to-code only; code nodes cannot point to documentary nodes"},
    {CBM_REFUSAL_LOG_REF_IMMUTABLE, "LOG_REF_IMMUTABLE", "log-anchored claims cannot drift; use epistemic supersession or recall instead"},
    {CBM_REFUSAL_PROVENANCE_UNDECLARED, "PROVENANCE_UNDECLARED", "cite governing theme node or explicitly declare invention with rationale; silent invention is refused"},
    {CBM_REFUSAL_BINDING_SELF_VALIDATED, "BINDING_SELF_VALIDATED", "theme bindings are sovereign to the operator; obtain operator validation before admission"},
    {CBM_REFUSAL_TERRITORY_WRITE_FORBIDDEN, "TERRITORY_WRITE_FORBIDDEN", "thematic graphs are read-only to project sessions; mutate via curator proposal or external flow"},
    {CBM_REFUSAL_AUTO_FOUNDING_FORBIDDEN, "AUTO_FOUNDING_FORBIDDEN", "autonomous founding of theme graphs is forbidden; offer proposal to operator at closure sweep"},
    {CBM_REFUSAL_THEME_SCHEMA_INVALID, "THEME_SCHEMA_INVALID", "correct the theme schema fields; theme_id, namespace, curator, and version are mandatory"},
    {CBM_REFUSAL_THEME_UNKNOWN, "THEME_UNKNOWN", "theme identifier or namespace does not exist in registry; verify or register theme"},
    {CBM_REFUSAL_THEME_PERSISTENCE_FAILED, "THEME_PERSISTENCE_FAILED", "catalog write failed; registry change was rolled back, retry only after storage is available"},
    {CBM_REFUSAL_THEME_VERSION_IMMUTABLE, "THEME_VERSION_IMMUTABLE", "publish a new version for changed thematic content; an existing version is immutable"},
    {CBM_REFUSAL_BINDING_PERSISTENCE_FAILED, "BINDING_PERSISTENCE_FAILED", "binding write failed; ledger change was rolled back, restore storage before retry"},
};

static const CbmRefusalDef *find_def(CbmRefusalCode code) {
    if (code <= CBM_REFUSAL_OK || code >= CBM_REFUSAL_CODE_COUNT) return NULL;
    const CbmRefusalDef *def = &k_refusal_defs[code - 1];
    if (def->code != code) return NULL; /* table/enum drift — refuse to answer */
    return def;
}

const char *cbm_refusal_code_string(CbmRefusalCode code) {
    const CbmRefusalDef *def = find_def(code);
    return def ? def->name : "UNKNOWN";
}

CbmRefusalCode cbm_refusal_code_from_string(const char *name) {
    if (!name) return CBM_REFUSAL_OK;
    for (size_t i = 0; i < CBM_REFUSAL_CODE_COUNT; i++) {
        if (k_refusal_defs[i].code != CBM_REFUSAL_OK &&
            strcmp(k_refusal_defs[i].name, name) == 0) {
            return k_refusal_defs[i].code;
        }
    }
    return CBM_REFUSAL_OK;
}

const char *cbm_refusal_client_obligation(CbmRefusalCode code) {
    const CbmRefusalDef *def = find_def(code);
    return def ? def->obligation : "";
}

bool cbm_refusal_is_valid(CbmRefusalCode code) {
    return find_def(code) != NULL;
}

void cbm_refusal_emit(CbmRefusalCode code, const char *horizon_id, const char *reason) {
    cbm_log_warn("union.refusal",
                 "code", cbm_refusal_code_string(code),
                 "obligation", cbm_refusal_client_obligation(code),
                 "horizon_id", horizon_id ? horizon_id : "",
                 "reason", reason ? reason : "",
                 NULL);
}

void cbm_refusal_ledger_init(CbmRefusalLedger *ledger) {
    if (!ledger) return;
    memset(ledger, 0, sizeof(*ledger));
}

uint64_t cbm_refusal_fingerprint(const char *horizon_id, CbmRefusalCode code, const char *payload) {
    /* FNV-1a 64 — same family as the VisitedSet, so the union speaks one hash. */
    uint64_t hash = 1469598103934665603ULL;
    const char *parts[3] = {horizon_id ? horizon_id : "", NULL, payload ? payload : ""};
    char code_buf[4];
    code_buf[0] = (char)(code & 0xff);
    code_buf[1] = (char)((code >> 8) & 0xff);
    code_buf[2] = '\0';
    parts[1] = code_buf;

    for (size_t p = 0; p < 3; p++) {
        for (const char *s = parts[p]; *s; s++) {
            hash ^= (uint64_t)(unsigned char)*s;
            hash *= 1099511628211ULL;
        }
        hash ^= 0x1fULL; /* part separator: "a"+"bc" != "ab"+"c" */
        hash *= 1099511628211ULL;
    }
    /* 0 is the empty-slot marker; disambiguate a genuine 0 hash. */
    if (hash == 0) hash = 1;
    return hash;
}

bool cbm_refusal_ledger_check_and_record(CbmRefusalLedger *ledger,
                                         const char *horizon_id,
                                         CbmRefusalCode code,
                                         const char *payload,
                                         CbmRefusalCode *out_effective) {
    if (!ledger || !cbm_refusal_is_valid(code)) {
        if (out_effective) *out_effective = CBM_REFUSAL_OK;
        return false;
    }

    uint64_t fp = cbm_refusal_fingerprint(horizon_id, code, payload);

    for (size_t i = 0; i < ledger->count && i < CBM_REFUSAL_LEDGER_CAP; i++) {
        if (ledger->entries[i].fingerprint == fp && ledger->entries[i].code == code) {
            if (out_effective) *out_effective = CBM_REFUSAL_RETRY_IDENTICAL;
            cbm_refusal_emit(CBM_REFUSAL_RETRY_IDENTICAL, horizon_id,
                             "identical re-submission of a refused payload");
            return true;
        }
    }

    size_t slot = ledger->next_slot;
    ledger->entries[slot].fingerprint = fp;
    ledger->entries[slot].code = code;
    ledger->entries[slot].recorded_at = 0; /* caller-side clock policy; see A09 */
    ledger->next_slot = (ledger->next_slot + 1) % CBM_REFUSAL_LEDGER_CAP;
    if (ledger->count < CBM_REFUSAL_LEDGER_CAP) ledger->count++;

    if (out_effective) *out_effective = code;
    cbm_refusal_emit(code, horizon_id, payload);
    return false;
}
