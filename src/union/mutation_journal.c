#include "mutation_journal.h"

#include "../foundation/compat_fs.h"
#include "../foundation/platform.h"

#include <sqlite3.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#define CBM_MUTATION_JOURNAL_PATH_MAX 4096
#define CBM_MUTATION_JOURNAL_FILE "mutation-journal.sqlite"

static const char *const MUTATION_JOURNAL_SCHEMA =
    "CREATE TABLE IF NOT EXISTS mutation_write_journal ("
    "write_id TEXT PRIMARY KEY NOT NULL,"
    "horizon_id TEXT NOT NULL,"
    "host INTEGER NOT NULL,"
    "context_id TEXT NOT NULL,"
    "attempt_id TEXT NOT NULL,"
    "operation INTEGER NOT NULL,"
    "scope INTEGER NOT NULL,"
    "effect INTEGER NOT NULL,"
    "target_path TEXT NOT NULL,"
    "intent_key TEXT NOT NULL,"
    "grounding_kind INTEGER NOT NULL,"
    "reference TEXT NOT NULL,"
    "rationale TEXT NOT NULL,"
    "created_at INTEGER NOT NULL,"
    "outcome INTEGER NOT NULL CHECK(outcome BETWEEN 1 AND 4)"
    ");"
    "CREATE INDEX IF NOT EXISTS mutation_write_journal_session_idx "
    "ON mutation_write_journal(horizon_id, outcome);"
    "CREATE TABLE IF NOT EXISTS mutation_session_authority ("
    "horizon_id TEXT PRIMARY KEY NOT NULL,"
    "host INTEGER NOT NULL,"
    "context_id TEXT NOT NULL,"
    "intent_key TEXT NOT NULL,"
    "grounding_kind INTEGER NOT NULL,"
    "reference TEXT NOT NULL,"
    "rationale TEXT NOT NULL,"
    "active INTEGER NOT NULL CHECK(active IN (0,1))"
    ");"
    "CREATE TABLE IF NOT EXISTS mutation_session_target ("
    "horizon_id TEXT NOT NULL,"
    "operation INTEGER NOT NULL,"
    "path TEXT NOT NULL,"
    "PRIMARY KEY(horizon_id,operation,path)"
    ");"
    "CREATE UNIQUE INDEX IF NOT EXISTS mutation_session_active_context_idx "
    "ON mutation_session_authority(host,context_id) WHERE active=1;";

static CbmMutationJournalResult journal_exec(CbmMutationJournal *journal, const char *sql) {
    if (!journal || !journal->db || !sql) return CBM_MUTATION_JOURNAL_ERR_ARGUMENT;
    return sqlite3_exec(journal->db, sql, NULL, NULL, NULL) == SQLITE_OK
               ? CBM_MUTATION_JOURNAL_OK
               : CBM_MUTATION_JOURNAL_ERR_STORAGE;
}

static CbmMutationJournalResult journal_migrate_target_path(CbmMutationJournal *journal) {
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(journal->db, "PRAGMA table_info(mutation_write_journal);", -1,
                           &statement, NULL) != SQLITE_OK) {
        sqlite3_finalize(statement);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    bool found = false;
    int rc = SQLITE_OK;
    while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
        const unsigned char *name = sqlite3_column_text(statement, 1);
        if (name && strcmp((const char *)name, "target_path") == 0) {
            found = true;
            break;
        }
    }
    sqlite3_finalize(statement);
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    if (found) return CBM_MUTATION_JOURNAL_OK;
    return journal_exec(journal,
                        "ALTER TABLE mutation_write_journal ADD COLUMN target_path TEXT NOT NULL DEFAULT ''; ");
}

CbmMutationJournalResult cbm_mutation_journal_open(CbmMutationJournal *journal,
                                                   const char *database_path) {
    if (!journal || !database_path || !database_path[0]) {
        return CBM_MUTATION_JOURNAL_ERR_ARGUMENT;
    }
    journal->db = NULL;
    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(database_path, &journal->db, flags, NULL) != SQLITE_OK) {
        cbm_mutation_journal_close(journal);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    sqlite3_busy_timeout(journal->db, 500);
    if (journal_exec(journal, "PRAGMA journal_mode=WAL;") != CBM_MUTATION_JOURNAL_OK ||
        journal_exec(journal, "PRAGMA synchronous=FULL;") != CBM_MUTATION_JOURNAL_OK ||
        journal_exec(journal, "PRAGMA foreign_keys=ON;") != CBM_MUTATION_JOURNAL_OK ||
        journal_exec(journal, MUTATION_JOURNAL_SCHEMA) != CBM_MUTATION_JOURNAL_OK ||
        journal_migrate_target_path(journal) != CBM_MUTATION_JOURNAL_OK) {
        cbm_mutation_journal_close(journal);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    return CBM_MUTATION_JOURNAL_OK;
}

static bool journal_default_path(char *path, size_t path_size, const char **cache_out) {
    const char *cache_dir = cbm_resolve_cache_dir();
    if (!cache_dir || !cache_dir[0]) return false;
    int written = snprintf(path, path_size, "%s/%s", cache_dir,
                           CBM_MUTATION_JOURNAL_FILE);
    if (written <= 0 || (size_t)written >= path_size) return false;
    if (cache_out) *cache_out = cache_dir;
    return true;
}

CbmMutationJournalResult cbm_mutation_journal_open_default(CbmMutationJournal *journal) {
    char path[CBM_MUTATION_JOURNAL_PATH_MAX];
    const char *cache_dir = NULL;
    if (!journal_default_path(path, sizeof(path), &cache_dir)) {
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    if (!cbm_mkdir_p_ex(cache_dir, 0700, CBM_MKDIR_FOLLOW_OWNED)) {
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    return cbm_mutation_journal_open(journal, path);
}

void cbm_mutation_journal_close(CbmMutationJournal *journal) {
    if (!journal) return;
    if (journal->db) sqlite3_close(journal->db);
    journal->db = NULL;
}

static bool mutation_binding_matches(const CbmMutationSessionBinding *binding,
                                     const char *horizon_id,
                                     const CbmHostWorkContext *host_context,
                                     const CbmChangeGrounding *grounding) {
    return binding && horizon_id && host_context && grounding &&
           strcmp(binding->horizon_id, horizon_id) == 0 &&
           binding->host_context.host == host_context->host &&
           strcmp(binding->host_context.context_id, host_context->context_id) == 0 &&
           binding->grounding.kind == grounding->kind &&
           strcmp(binding->grounding.intent_key, grounding->intent_key) == 0 &&
           strcmp(binding->grounding.reference, grounding->reference) == 0 &&
           strcmp(binding->grounding.rationale, grounding->rationale) == 0 &&
           binding->grounding.target_count == grounding->target_count;
}

static bool mutation_targets_match(const CbmMutationSessionBinding *binding,
                                   const CbmChangeGrounding *grounding) {
    if (!binding || !grounding ||
        binding->grounding.target_count != grounding->target_count) return false;
    for (size_t wanted = 0; wanted < grounding->target_count; wanted++) {
        bool found = false;
        for (size_t existing = 0; existing < binding->grounding.target_count; existing++) {
            if (binding->grounding.targets[existing].operation ==
                    grounding->targets[wanted].operation &&
                strcmp(binding->grounding.targets[existing].path,
                       grounding->targets[wanted].path) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

CbmMutationJournalResult cbm_mutation_journal_find_session(
    CbmMutationJournal *journal, const CbmHostWorkContext *host_context,
    CbmMutationSessionBinding *binding_out, bool *other_host_context_out) {
    if (!journal || !journal->db || !cbm_host_work_context_is_valid(host_context) ||
        !binding_out) {
        return CBM_MUTATION_JOURNAL_ERR_ARGUMENT;
    }
    if (other_host_context_out) *other_host_context_out = false;
    memset(binding_out, 0, sizeof(*binding_out));
    static const char find_sql[] =
        "SELECT horizon_id,intent_key,grounding_kind,reference,rationale FROM "
        "mutation_session_authority WHERE host=? AND context_id=? AND active=1;";
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(journal->db, find_sql, -1, &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_int(statement, 1, (int)host_context->host) != SQLITE_OK ||
        sqlite3_bind_text(statement, 2, host_context->context_id, -1, SQLITE_TRANSIENT) !=
            SQLITE_OK) {
        sqlite3_finalize(statement);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    int rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW) {
        const unsigned char *horizon = sqlite3_column_text(statement, 0);
        const unsigned char *intent = sqlite3_column_text(statement, 1);
        const unsigned char *reference = sqlite3_column_text(statement, 3);
        const unsigned char *rationale = sqlite3_column_text(statement, 4);
        if (!horizon || !intent || !reference || !rationale ||
            sqlite3_column_bytes(statement, 0) >= (int)sizeof(binding_out->horizon_id) ||
            sqlite3_column_bytes(statement, 1) >= (int)sizeof(binding_out->grounding.intent_key) ||
            sqlite3_column_bytes(statement, 3) >= (int)sizeof(binding_out->grounding.reference) ||
            sqlite3_column_bytes(statement, 4) >= (int)sizeof(binding_out->grounding.rationale)) {
            sqlite3_finalize(statement);
            return CBM_MUTATION_JOURNAL_ERR_STORAGE;
        }
        snprintf(binding_out->horizon_id, sizeof(binding_out->horizon_id), "%s", horizon);
        binding_out->host_context = *host_context;
        binding_out->grounding.kind = (CbmGroundingKind)sqlite3_column_int(statement, 2);
        snprintf(binding_out->grounding.intent_key, sizeof(binding_out->grounding.intent_key),
                 "%s", intent);
        snprintf(binding_out->grounding.reference, sizeof(binding_out->grounding.reference),
                 "%s", reference);
        snprintf(binding_out->grounding.rationale, sizeof(binding_out->grounding.rationale),
                 "%s", rationale);
        sqlite3_finalize(statement);
        statement = NULL;
        if (sqlite3_prepare_v2(journal->db,
                               "SELECT operation,path FROM mutation_session_target "
                               "WHERE horizon_id=? ORDER BY operation,path;",
                               -1, &statement, NULL) != SQLITE_OK ||
            sqlite3_bind_text(statement, 1, binding_out->horizon_id, -1,
                              SQLITE_TRANSIENT) != SQLITE_OK) {
            sqlite3_finalize(statement);
            return CBM_MUTATION_JOURNAL_ERR_STORAGE;
        }
        while ((rc = sqlite3_step(statement)) == SQLITE_ROW) {
            const unsigned char *path = sqlite3_column_text(statement, 1);
            if (!path || binding_out->grounding.target_count >=
                             CBM_MUTATION_SCOPE_MAX_TARGETS ||
                sqlite3_column_bytes(statement, 1) >=
                    (int)sizeof(binding_out->grounding.targets[0].path)) {
                sqlite3_finalize(statement);
                return CBM_MUTATION_JOURNAL_ERR_STORAGE;
            }
            CbmMutationIntentTarget *target =
                &binding_out->grounding.targets[binding_out->grounding.target_count++];
            target->operation = (CbmMutationOperation)sqlite3_column_int(statement, 0);
            snprintf(target->path, sizeof(target->path), "%s", path);
        }
        sqlite3_finalize(statement);
        return rc == SQLITE_DONE && cbm_change_grounding_is_valid(&binding_out->grounding)
                   ? CBM_MUTATION_JOURNAL_OK
                   : CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    sqlite3_finalize(statement);
    if (rc != SQLITE_DONE) return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    if (other_host_context_out) {
        if (sqlite3_prepare_v2(journal->db,
                               "SELECT 1 FROM mutation_session_authority WHERE host=? AND active=1 LIMIT 1;",
                               -1, &statement, NULL) != SQLITE_OK ||
            sqlite3_bind_int(statement, 1, (int)host_context->host) != SQLITE_OK) {
            sqlite3_finalize(statement);
            return CBM_MUTATION_JOURNAL_ERR_STORAGE;
        }
        rc = sqlite3_step(statement);
        *other_host_context_out = rc == SQLITE_ROW;
        sqlite3_finalize(statement);
        if (rc != SQLITE_ROW && rc != SQLITE_DONE) return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    return CBM_MUTATION_JOURNAL_ERR_NOT_FOUND;
}

CbmMutationJournalResult cbm_mutation_journal_register_session(
    CbmMutationJournal *journal, const char *horizon_id,
    const CbmHostWorkContext *host_context, const CbmChangeGrounding *grounding) {
    if (!journal || !journal->db || !horizon_id || !horizon_id[0] ||
        !cbm_host_work_context_is_valid(host_context) ||
        !cbm_change_grounding_is_valid(grounding)) {
        return CBM_MUTATION_JOURNAL_ERR_ARGUMENT;
    }
    for (size_t index = 0; index < grounding->target_count; index++) {
        char canonical[CBM_MUTATION_TARGET_PATH_MAX];
        if (!cbm_mutation_canonicalize_path(grounding->targets[index].path, canonical,
                                            sizeof(canonical)) ||
            strcmp(canonical, grounding->targets[index].path) != 0) {
            return CBM_MUTATION_JOURNAL_ERR_ARGUMENT;
        }
    }
    static const char insert_sql[] =
        "INSERT OR IGNORE INTO mutation_session_authority "
        "(horizon_id,host,context_id,intent_key,grounding_kind,reference,rationale,active) "
        "VALUES (?,?,?,?,?,?,?,1);";
    if (journal_exec(journal, "BEGIN IMMEDIATE;") != CBM_MUTATION_JOURNAL_OK) {
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    sqlite3_stmt *statement = NULL;
    int rc = sqlite3_prepare_v2(journal->db, insert_sql, -1, &statement, NULL);
    if (rc == SQLITE_OK) {
        rc |= sqlite3_bind_text(statement, 1, horizon_id, -1, SQLITE_TRANSIENT);
        rc |= sqlite3_bind_int(statement, 2, (int)host_context->host);
        rc |= sqlite3_bind_text(statement, 3, host_context->context_id, -1, SQLITE_TRANSIENT);
        rc |= sqlite3_bind_text(statement, 4, grounding->intent_key, -1, SQLITE_TRANSIENT);
        rc |= sqlite3_bind_int(statement, 5, (int)grounding->kind);
        rc |= sqlite3_bind_text(statement, 6, grounding->reference, -1, SQLITE_TRANSIENT);
        rc |= sqlite3_bind_text(statement, 7, grounding->rationale, -1, SQLITE_TRANSIENT);
    }
    bool inserted_or_existing = rc == SQLITE_OK && sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    if (!inserted_or_existing) {
        (void)sqlite3_exec(journal->db, "ROLLBACK;", NULL, NULL, NULL);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    static const char insert_target_sql[] =
        "INSERT OR IGNORE INTO mutation_session_target (horizon_id,operation,path) "
        "VALUES (?,?,?);";
    if (sqlite3_prepare_v2(journal->db, insert_target_sql, -1, &statement, NULL) != SQLITE_OK) {
        (void)sqlite3_exec(journal->db, "ROLLBACK;", NULL, NULL, NULL);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    bool targets_inserted = true;
    for (size_t index = 0; index < grounding->target_count; index++) {
        int bind_rc = sqlite3_bind_text(statement, 1, horizon_id, -1, SQLITE_TRANSIENT);
        bind_rc |= sqlite3_bind_int(statement, 2, (int)grounding->targets[index].operation);
        bind_rc |= sqlite3_bind_text(statement, 3, grounding->targets[index].path, -1,
                                     SQLITE_TRANSIENT);
        if (bind_rc != SQLITE_OK || sqlite3_step(statement) != SQLITE_DONE) {
            targets_inserted = false;
            break;
        }
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
    }
    sqlite3_finalize(statement);
    if (!targets_inserted) {
        (void)sqlite3_exec(journal->db, "ROLLBACK;", NULL, NULL, NULL);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    CbmMutationSessionBinding existing = {0};
    bool other_context = false;
    CbmMutationJournalResult found = cbm_mutation_journal_find_session(
        journal, host_context, &existing, &other_context);
    CbmMutationJournalResult result =
        found == CBM_MUTATION_JOURNAL_OK &&
                mutation_binding_matches(&existing, horizon_id, host_context, grounding) &&
                mutation_targets_match(&existing, grounding)
            ? CBM_MUTATION_JOURNAL_OK
            : CBM_MUTATION_JOURNAL_ERR_STORAGE;
    if (result == CBM_MUTATION_JOURNAL_OK) result = journal_exec(journal, "COMMIT;");
    if (result != CBM_MUTATION_JOURNAL_OK) {
        (void)sqlite3_exec(journal->db, "ROLLBACK;", NULL, NULL, NULL);
    }
    return result;
}

CbmMutationJournalResult cbm_mutation_journal_register_default_session(
    const char *horizon_id, const CbmHostWorkContext *host_context,
    const CbmChangeGrounding *grounding) {
    CbmMutationJournal journal = {0};
    CbmMutationJournalResult result = cbm_mutation_journal_open_default(&journal);
    if (result == CBM_MUTATION_JOURNAL_OK) {
        result = cbm_mutation_journal_register_session(&journal, horizon_id,
                                                      host_context, grounding);
    }
    cbm_mutation_journal_close(&journal);
    return result;
}

static bool write_id_generate(char *write_id, size_t capacity) {
    unsigned char random[16];
    if (!write_id || capacity < 37U) return false;
    sqlite3_randomness((int)sizeof(random), random);
    int written = snprintf(write_id, capacity,
                           "wr_%02x%02x%02x%02x%02x%02x%02x%02x"
                           "%02x%02x%02x%02x%02x%02x%02x%02x",
                           random[0], random[1], random[2], random[3], random[4], random[5],
                           random[6], random[7], random[8], random[9], random[10], random[11],
                           random[12], random[13], random[14], random[15]);
    return written > 0 && (size_t)written < capacity;
}

static bool journal_session_is_active(CbmMutationJournal *journal, const char *horizon_id,
                                      const CbmMutationAttempt *attempt,
                                      const CbmChangeGrounding *grounding) {
    static const char sql[] =
        "SELECT 1 FROM mutation_session_authority a JOIN mutation_session_target t "
        "ON t.horizon_id=a.horizon_id WHERE a.horizon_id=? AND a.host=? AND "
        "a.context_id=? AND a.intent_key=? AND a.grounding_kind=? AND a.reference=? "
        "AND a.rationale=? AND a.active=1 AND t.operation=? AND t.path=?;";
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(journal->db, sql, -1, &statement, NULL) != SQLITE_OK) return false;
    int rc = sqlite3_bind_text(statement, 1, horizon_id, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_int(statement, 2, (int)attempt->host_context.host);
    rc |= sqlite3_bind_text(statement, 3, attempt->host_context.context_id, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(statement, 4, grounding->intent_key, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_int(statement, 5, (int)grounding->kind);
    rc |= sqlite3_bind_text(statement, 6, grounding->reference, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(statement, 7, grounding->rationale, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_int(statement, 8, (int)attempt->operation);
    rc |= sqlite3_bind_text(statement, 9, attempt->target_path, -1, SQLITE_TRANSIENT);
    bool active = rc == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_finalize(statement);
    return active;
}

CbmMutationJournalResult cbm_mutation_journal_commit_intent(
    CbmMutationJournal *journal, const char *horizon_id, const CbmMutationAttempt *attempt,
    const CbmChangeGrounding *grounding, char *write_id_out, size_t write_id_out_size) {
    if (!journal || !journal->db || !horizon_id || !horizon_id[0] || !attempt ||
        !cbm_host_work_context_is_valid(&attempt->host_context) ||
        !cbm_change_grounding_is_valid(grounding) || !attempt->target_path[0] ||
        !memchr(attempt->target_path, '\0', sizeof(attempt->target_path)) ||
        !write_id_out || write_id_out_size < 37U) {
        return CBM_MUTATION_JOURNAL_ERR_ARGUMENT;
    }
    char canonical_target[CBM_MUTATION_TARGET_PATH_MAX];
    if (!cbm_mutation_canonicalize_path(attempt->target_path, canonical_target,
                                        sizeof(canonical_target)) ||
        strcmp(canonical_target, attempt->target_path) != 0) {
        return CBM_MUTATION_JOURNAL_ERR_ARGUMENT;
    }
    CbmMutationEffect effect = cbm_mutation_classify(attempt->operation, attempt->scope);
    if (effect != CBM_MUTATION_EFFECT_REPOSITORY_WRITE &&
        effect != CBM_MUTATION_EFFECT_AMBIGUOUS) {
        return CBM_MUTATION_JOURNAL_ERR_ARGUMENT;
    }

    char write_id[CBM_MUTATION_WRITE_ID_MAX] = {0};
    if (!write_id_generate(write_id, sizeof(write_id))) {
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    static const char insert_sql[] =
        "INSERT INTO mutation_write_journal (write_id,horizon_id,host,context_id,attempt_id,"
        "operation,scope,effect,target_path,intent_key,grounding_kind,reference,rationale,created_at,outcome) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";
    sqlite3_stmt *statement = NULL;
    CbmMutationJournalResult result = CBM_MUTATION_JOURNAL_ERR_STORAGE;
    if (journal_exec(journal, "BEGIN IMMEDIATE;") != CBM_MUTATION_JOURNAL_OK) {
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    /* Recheck the authority row while holding the write transaction. A
     * concurrent session close cannot race a stale read into a new permit. */
    if (!journal_session_is_active(journal, horizon_id, attempt, grounding) ||
        sqlite3_prepare_v2(journal->db, insert_sql, -1, &statement, NULL) != SQLITE_OK) {
        (void)sqlite3_exec(journal->db, "ROLLBACK;", NULL, NULL, NULL);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    int rc = SQLITE_OK;
    rc |= sqlite3_bind_text(statement, 1, write_id, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(statement, 2, horizon_id, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_int(statement, 3, (int)attempt->host_context.host);
    rc |= sqlite3_bind_text(statement, 4, attempt->host_context.context_id, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(statement, 5, attempt->attempt_id, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_int(statement, 6, (int)attempt->operation);
    rc |= sqlite3_bind_int(statement, 7, (int)attempt->scope);
    rc |= sqlite3_bind_int(statement, 8, (int)effect);
    rc |= sqlite3_bind_text(statement, 9, attempt->target_path, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(statement, 10, grounding->intent_key, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_int(statement, 11, (int)grounding->kind);
    rc |= sqlite3_bind_text(statement, 12, grounding->reference, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_text(statement, 13, grounding->rationale, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_int64(statement, 14, (sqlite3_int64)time(NULL));
    rc |= sqlite3_bind_int(statement, 15, CBM_MUTATION_OUTCOME_PENDING);
    if (rc == SQLITE_OK && sqlite3_step(statement) == SQLITE_DONE &&
        journal_exec(journal, "COMMIT;") == CBM_MUTATION_JOURNAL_OK) {
        snprintf(write_id_out, write_id_out_size, "%s", write_id);
        result = CBM_MUTATION_JOURNAL_OK;
    } else {
        (void)sqlite3_exec(journal->db, "ROLLBACK;", NULL, NULL, NULL);
    }
    sqlite3_finalize(statement);
    return result;
}

CbmMutationJournalResult cbm_mutation_journal_get_outcome(CbmMutationJournal *journal,
                                                          const char *write_id,
                                                          CbmMutationOutcome *outcome) {
    if (!journal || !journal->db || !write_id || !write_id[0] || !outcome) {
        return CBM_MUTATION_JOURNAL_ERR_ARGUMENT;
    }
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(journal->db,
                           "SELECT outcome FROM mutation_write_journal WHERE write_id=?;", -1,
                           &statement, NULL) != SQLITE_OK ||
        sqlite3_bind_text(statement, 1, write_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(statement);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    int rc = sqlite3_step(statement);
    if (rc == SQLITE_ROW) {
        *outcome = (CbmMutationOutcome)sqlite3_column_int(statement, 0);
        sqlite3_finalize(statement);
        return CBM_MUTATION_JOURNAL_OK;
    }
    sqlite3_finalize(statement);
    return rc == SQLITE_DONE ? CBM_MUTATION_JOURNAL_ERR_NOT_FOUND
                             : CBM_MUTATION_JOURNAL_ERR_STORAGE;
}

CbmMutationJournalResult cbm_mutation_journal_observe(CbmMutationJournal *journal,
                                                      const char *write_id,
                                                      CbmMutationOutcome outcome) {
    if (!journal || !journal->db || !write_id || !write_id[0] ||
        (outcome != CBM_MUTATION_OUTCOME_OBSERVED_APPLIED &&
         outcome != CBM_MUTATION_OUTCOME_OBSERVED_FAILED &&
         outcome != CBM_MUTATION_OUTCOME_UNKNOWN)) {
        return CBM_MUTATION_JOURNAL_ERR_ARGUMENT;
    }
    sqlite3_stmt *statement = NULL;
    if (sqlite3_prepare_v2(journal->db,
                           "UPDATE mutation_write_journal SET outcome=? "
                           "WHERE write_id=? AND outcome=?;",
                           -1, &statement, NULL) != SQLITE_OK) {
        sqlite3_finalize(statement);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    int rc = sqlite3_bind_int(statement, 1, (int)outcome);
    rc |= sqlite3_bind_text(statement, 2, write_id, -1, SQLITE_TRANSIENT);
    rc |= sqlite3_bind_int(statement, 3, CBM_MUTATION_OUTCOME_PENDING);
    if (rc != SQLITE_OK || sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    int changed = sqlite3_changes(journal->db);
    sqlite3_finalize(statement);
    if (changed == 1) return CBM_MUTATION_JOURNAL_OK;

    CbmMutationOutcome stored = CBM_MUTATION_OUTCOME_NONE;
    CbmMutationJournalResult found = cbm_mutation_journal_get_outcome(journal, write_id, &stored);
    if (found != CBM_MUTATION_JOURNAL_OK) return found;
    return stored == outcome ? CBM_MUTATION_JOURNAL_OK
                             : CBM_MUTATION_JOURNAL_ERR_OUTCOME_CONFLICT;
}

static CbmMutationJournalResult mark_pending_unknown(CbmMutationJournal *journal,
                                                     const char *horizon_id) {
    sqlite3_stmt *statement = NULL;
    const char *sql = horizon_id
        ? "UPDATE mutation_write_journal SET outcome=? WHERE outcome=? AND horizon_id=?;"
        : "UPDATE mutation_write_journal SET outcome=? WHERE outcome=?;";
    if (sqlite3_prepare_v2(journal->db, sql, -1, &statement, NULL) != SQLITE_OK) {
        sqlite3_finalize(statement);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    int rc = sqlite3_bind_int(statement, 1, CBM_MUTATION_OUTCOME_UNKNOWN);
    rc |= sqlite3_bind_int(statement, 2, CBM_MUTATION_OUTCOME_PENDING);
    if (horizon_id) rc |= sqlite3_bind_text(statement, 3, horizon_id, -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK || sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    sqlite3_finalize(statement);
    statement = NULL;
    const char *deactivate_sql = horizon_id
        ? "UPDATE mutation_session_authority SET active=0 WHERE horizon_id=?;"
        : "UPDATE mutation_session_authority SET active=0 WHERE active=1;";
    if (sqlite3_prepare_v2(journal->db, deactivate_sql, -1, &statement, NULL) != SQLITE_OK) {
        sqlite3_finalize(statement);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    if (horizon_id &&
        sqlite3_bind_text(statement, 1, horizon_id, -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        sqlite3_finalize(statement);
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    int deactivate_rc = sqlite3_step(statement);
    sqlite3_finalize(statement);
    return deactivate_rc == SQLITE_DONE ? CBM_MUTATION_JOURNAL_OK
                                        : CBM_MUTATION_JOURNAL_ERR_STORAGE;
}

CbmMutationJournalResult cbm_mutation_journal_mark_session_unknown(
    CbmMutationJournal *journal, const char *horizon_id) {
    if (!journal || !journal->db || !horizon_id || !horizon_id[0]) {
        return CBM_MUTATION_JOURNAL_ERR_ARGUMENT;
    }
    if (journal_exec(journal, "BEGIN IMMEDIATE;") != CBM_MUTATION_JOURNAL_OK) {
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    CbmMutationJournalResult result = mark_pending_unknown(journal, horizon_id);
    if (result == CBM_MUTATION_JOURNAL_OK) result = journal_exec(journal, "COMMIT;");
    if (result != CBM_MUTATION_JOURNAL_OK) (void)sqlite3_exec(journal->db, "ROLLBACK;", NULL, NULL, NULL);
    return result;
}

CbmMutationJournalResult cbm_mutation_journal_recover_pending(CbmMutationJournal *journal) {
    if (!journal || !journal->db) return CBM_MUTATION_JOURNAL_ERR_ARGUMENT;
    if (journal_exec(journal, "BEGIN IMMEDIATE;") != CBM_MUTATION_JOURNAL_OK) {
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    CbmMutationJournalResult result = mark_pending_unknown(journal, NULL);
    if (result == CBM_MUTATION_JOURNAL_OK) result = journal_exec(journal, "COMMIT;");
    if (result != CBM_MUTATION_JOURNAL_OK) (void)sqlite3_exec(journal->db, "ROLLBACK;", NULL, NULL, NULL);
    return result;
}

CbmMutationJournalResult cbm_mutation_journal_recover_default_if_present(void) {
    char path[CBM_MUTATION_JOURNAL_PATH_MAX];
    if (!journal_default_path(path, sizeof(path), NULL)) {
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    if (!cbm_file_exists(path)) return CBM_MUTATION_JOURNAL_OK;
    CbmMutationJournal journal = {0};
    CbmMutationJournalResult result = cbm_mutation_journal_open(&journal, path);
    if (result == CBM_MUTATION_JOURNAL_OK) {
        result = cbm_mutation_journal_recover_pending(&journal);
    }
    cbm_mutation_journal_close(&journal);
    return result;
}

CbmMutationJournalResult cbm_mutation_journal_mark_default_session_unknown(
    const char *horizon_id) {
    if (!horizon_id || !horizon_id[0]) return CBM_MUTATION_JOURNAL_ERR_ARGUMENT;
    char path[CBM_MUTATION_JOURNAL_PATH_MAX];
    if (!journal_default_path(path, sizeof(path), NULL)) {
        return CBM_MUTATION_JOURNAL_ERR_STORAGE;
    }
    if (!cbm_file_exists(path)) return CBM_MUTATION_JOURNAL_OK;
    CbmMutationJournal journal = {0};
    CbmMutationJournalResult result = cbm_mutation_journal_open(&journal, path);
    if (result == CBM_MUTATION_JOURNAL_OK) {
        result = cbm_mutation_journal_mark_session_unknown(&journal, horizon_id);
    }
    cbm_mutation_journal_close(&journal);
    return result;
}
