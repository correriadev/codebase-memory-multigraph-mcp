/* Durable per-write intent and outcome journal for E01. */
#ifndef CBM_UNION_MUTATION_JOURNAL_H
#define CBM_UNION_MUTATION_JOURNAL_H

#include "mutation_gate.h"
#include "../core/horizon_pool.h"

#include <stddef.h>

typedef struct sqlite3 sqlite3;

typedef enum {
    CBM_MUTATION_JOURNAL_OK = 0,
    CBM_MUTATION_JOURNAL_ERR_ARGUMENT = -1,
    CBM_MUTATION_JOURNAL_ERR_STORAGE = -2,
    CBM_MUTATION_JOURNAL_ERR_NOT_FOUND = -3,
    CBM_MUTATION_JOURNAL_ERR_OUTCOME_CONFLICT = -4
} CbmMutationJournalResult;

typedef enum {
    CBM_MUTATION_OUTCOME_NONE = 0,
    CBM_MUTATION_OUTCOME_PENDING = 1,
    CBM_MUTATION_OUTCOME_OBSERVED_APPLIED = 2,
    CBM_MUTATION_OUTCOME_OBSERVED_FAILED = 3,
    CBM_MUTATION_OUTCOME_UNKNOWN = 4
} CbmMutationOutcome;

typedef struct CbmMutationJournal {
    sqlite3 *db;
} CbmMutationJournal;

typedef struct {
    char horizon_id[CBM_HORIZON_ID_MAX];
    CbmHostWorkContext host_context;
    CbmChangeGrounding grounding;
} CbmMutationSessionBinding;

CbmMutationJournalResult cbm_mutation_journal_open(CbmMutationJournal *journal,
                                                   const char *database_path);
CbmMutationJournalResult cbm_mutation_journal_open_default(CbmMutationJournal *journal);
void cbm_mutation_journal_close(CbmMutationJournal *journal);

/* Session authority is shared between the MCP daemon and short-lived hooks. */
CbmMutationJournalResult cbm_mutation_journal_register_session(
    CbmMutationJournal *journal, const char *horizon_id,
    const CbmHostWorkContext *host_context, const CbmChangeGrounding *grounding);
CbmMutationJournalResult cbm_mutation_journal_register_default_session(
    const char *horizon_id, const CbmHostWorkContext *host_context,
    const CbmChangeGrounding *grounding);
CbmMutationJournalResult cbm_mutation_journal_find_session(
    CbmMutationJournal *journal, const CbmHostWorkContext *host_context,
    CbmMutationSessionBinding *binding_out, bool *other_host_context_out);

/* The intent row is committed with synchronous=FULL before this returns OK. */
CbmMutationJournalResult cbm_mutation_journal_commit_intent(
    CbmMutationJournal *journal, const char *horizon_id, const CbmMutationAttempt *attempt,
    const CbmChangeGrounding *grounding, char *write_id_out, size_t write_id_out_size);

/* Outcome delivery is idempotent for an identical result and rejects conflicts. */
CbmMutationJournalResult cbm_mutation_journal_observe(CbmMutationJournal *journal,
                                                      const char *write_id,
                                                      CbmMutationOutcome outcome);
CbmMutationJournalResult cbm_mutation_journal_get_outcome(CbmMutationJournal *journal,
                                                          const char *write_id,
                                                          CbmMutationOutcome *outcome);

/* Any permit without a trustworthy result becomes UNKNOWN at session closure
 * or after a daemon restart. It is never inferred to have been applied. */
CbmMutationJournalResult cbm_mutation_journal_mark_session_unknown(
    CbmMutationJournal *journal, const char *horizon_id);
CbmMutationJournalResult cbm_mutation_journal_recover_pending(CbmMutationJournal *journal);
CbmMutationJournalResult cbm_mutation_journal_recover_default_if_present(void);
CbmMutationJournalResult cbm_mutation_journal_mark_default_session_unknown(
    const char *horizon_id);

#endif /* CBM_UNION_MUTATION_JOURNAL_H */
