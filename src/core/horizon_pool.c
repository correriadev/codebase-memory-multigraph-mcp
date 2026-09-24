#include "horizon_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
  #include <direct.h>
  #include <io.h>
  #define cbm_mkdir(path) _mkdir(path)
  #define cbm_unlink(path) _unlink(path)
#else
  #include <unistd.h>
  #define cbm_mkdir(path) mkdir(path, 0755)
  #define cbm_unlink(path) unlink(path)
#endif

static const char *HORIZON_DDL =
    "PRAGMA journal_mode = WAL;\n"
    "PRAGMA synchronous = NORMAL;\n"
    "PRAGMA foreign_keys = ON;\n"
    "CREATE TABLE IF NOT EXISTS horizon_metadata (\n"
    "    horizon_id TEXT PRIMARY KEY,\n"
    "    client_pid INTEGER NOT NULL,\n"
    "    status TEXT NOT NULL CHECK(status IN ('ACTIVE', 'PROMOTED', 'DISCARDED')),\n"
    "    created_at INTEGER NOT NULL,\n"
    "    last_heartbeat INTEGER NOT NULL,\n"
    "    based_on_seq TEXT NOT NULL DEFAULT '0'\n"
    ");\n"
    "CREATE TABLE IF NOT EXISTS symbolic_nodes (\n"
    "    cbm_uri TEXT PRIMARY KEY,\n"
    "    label TEXT NOT NULL,\n"
    "    epistemic_status TEXT NOT NULL DEFAULT 'PROPOSED' CHECK(epistemic_status IN ('PROPOSED', 'ACCEPTED', 'CONTESTED', 'SHADOWED')),\n"
    "    is_dangling INTEGER NOT NULL DEFAULT 0,\n"
    "    code_snippet TEXT,\n"
    "    created_at INTEGER NOT NULL\n"
    ");\n"
    "CREATE INDEX IF NOT EXISTS idx_symbolic_nodes_label ON symbolic_nodes(label);\n"
    "CREATE INDEX IF NOT EXISTS idx_symbolic_nodes_status ON symbolic_nodes(epistemic_status);\n"
    "CREATE TABLE IF NOT EXISTS virtual_edges (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    source_uri TEXT NOT NULL,\n"
    "    target_uri TEXT NOT NULL,\n"
    "    edge_type TEXT NOT NULL,\n"
    "    origin_horizon TEXT NOT NULL,\n"
    "    created_at INTEGER NOT NULL,\n"
    "    UNIQUE(source_uri, target_uri, edge_type)\n"
    ");\n"
    "CREATE INDEX IF NOT EXISTS idx_virtual_edges_source ON virtual_edges(source_uri);\n"
    "CREATE INDEX IF NOT EXISTS idx_virtual_edges_target ON virtual_edges(target_uri);\n"
    "CREATE VIRTUAL TABLE IF NOT EXISTS spec_fts USING fts5(\n"
    "    file_path UNINDEXED,\n"
    "    heading_slug,\n"
    "    title,\n"
    "    content,\n"
    "    tokenize = 'porter unicode61'\n"
    ");\n";

static bool is_valid_horizon_id(const char *id) {
    if (!id || !id[0]) return false;
    for (const char *p = id; *p; p++) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}

static int make_horizon_path(const HorizonConnectionPool *pool, const char *horizon_id, char *out_path, size_t out_sz) {
    if (!pool || !is_valid_horizon_id(horizon_id) || !out_path || out_sz == 0) {
        if (out_path && out_sz > 0) out_path[0] = '\0';
        return -1;
    }

    if (pool->base_dir[0]) {
        snprintf(out_path, out_sz, "%s/horizons/%s.db", pool->base_dir, horizon_id);
    } else {
        snprintf(out_path, out_sz, "horizons/%s.db", horizon_id);
    }
    return 0;
}

static void ensure_directories(const char *path) {
    char temp[CBM_PATH_MAX];
    snprintf(temp, sizeof(temp), "%s", path);
    for (char *p = temp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            cbm_mkdir(temp);
            *p = '/';
        }
    }
}

int cbm_horizon_pool_init(HorizonConnectionPool *pool, const char *storage_dir) {
    if (!pool) return -1;
    memset(pool, 0, sizeof(*pool));
    if (storage_dir && storage_dir[0]) {
        snprintf(pool->base_dir, sizeof(pool->base_dir), "%s", storage_dir);
    } else {
        pool->base_dir[0] = '\0';
    }
    return 0;
}

void cbm_horizon_pool_close_all(HorizonConnectionPool *pool) {
    if (!pool) return;
    for (size_t i = 0; i < CBM_MAX_HORIZON_FDS; i++) {
        if (pool->open_handles[i]) {
            sqlite3_close_v2(pool->open_handles[i]);
            pool->open_handles[i] = NULL;
            pool->active_ids[i][0] = '\0';
            pool->lru_ticks[i] = 0;
        }
    }
    pool->count = 0;
}

int cbm_horizon_pool_invalidate(HorizonConnectionPool *pool, const char *horizon_id) {
    if (!pool || !horizon_id) return -1;
    for (size_t i = 0; i < CBM_MAX_HORIZON_FDS; i++) {
        if (pool->open_handles[i] && strcmp(pool->active_ids[i], horizon_id) == 0) {
            sqlite3_close_v2(pool->open_handles[i]);
            pool->open_handles[i] = NULL;
            pool->active_ids[i][0] = '\0';
            pool->lru_ticks[i] = 0;
            pool->count--;
            return 0;
        }
    }
    return 0;
}

int cbm_create_horizon(HorizonConnectionPool *pool, uint32_t client_pid, const char *custom_id, const char *based_on_seq, char *out_id, size_t out_sz) {
    if (!pool || !out_id || out_sz == 0) return -1;

    char horizon_id[CBM_HORIZON_ID_MAX];
    if (custom_id && custom_id[0]) {
        if (!is_valid_horizon_id(custom_id)) return -1;
        snprintf(horizon_id, sizeof(horizon_id), "%s", custom_id);
    } else {
        snprintf(horizon_id, sizeof(horizon_id), "horizon_%lu_%u", (unsigned long)time(NULL), (unsigned int)rand() % 100000);
    }

    char db_path[CBM_PATH_MAX];
    if (make_horizon_path(pool, horizon_id, db_path, sizeof(db_path)) != 0) {
        return -1;
    }
    ensure_directories(db_path);

    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close_v2(db);
        return -1;
    }

    char *err_msg = NULL;
    rc = sqlite3_exec(db, HORIZON_DDL, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) sqlite3_free(err_msg);
        sqlite3_close_v2(db);
        return -1;
    }

    uint64_t now = (uint64_t)time(NULL);
    sqlite3_stmt *stmt = NULL;
    const char *insert_meta =
        "INSERT INTO horizon_metadata (horizon_id, client_pid, status, created_at, last_heartbeat, based_on_seq) "
        "VALUES (?, ?, 'ACTIVE', ?, ?, ?) "
        "ON CONFLICT(horizon_id) DO UPDATE SET status = 'ACTIVE', last_heartbeat = excluded.last_heartbeat, based_on_seq = excluded.based_on_seq;";
    rc = sqlite3_prepare_v2(db, insert_meta, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, horizon_id, -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64)client_pid);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64)now);
        sqlite3_bind_int64(stmt, 4, (sqlite3_int64)now);
        sqlite3_bind_text(stmt, 5, based_on_seq ? based_on_seq : "0", -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    sqlite3_close_v2(db);
    snprintf(out_id, out_sz, "%s", horizon_id);
    return 0;
}

int cbm_horizon_pool_get(HorizonConnectionPool *pool, const char *horizon_id, sqlite3 **out_db) {
    if (!pool || !horizon_id || !out_db || !is_valid_horizon_id(horizon_id)) return -1;

    pool->current_tick++;

    /* 1. Check if already open */
    for (size_t i = 0; i < CBM_MAX_HORIZON_FDS; i++) {
        if (pool->open_handles[i] && strcmp(pool->active_ids[i], horizon_id) == 0) {
            pool->lru_ticks[i] = pool->current_tick;
            *out_db = pool->open_handles[i];
            return 0;
        }
    }

    /* 2. Need to open DB. Find slot (empty slot or LRU eviction) */
    size_t target_slot = 0;
    if (pool->count < CBM_MAX_HORIZON_FDS) {
        for (size_t i = 0; i < CBM_MAX_HORIZON_FDS; i++) {
            if (!pool->open_handles[i]) {
                target_slot = i;
                break;
            }
        }
    } else {
        /* Pool is full -> evict oldest LRU */
        uint64_t oldest_tick = UINT64_MAX;
        for (size_t i = 0; i < CBM_MAX_HORIZON_FDS; i++) {
            if (pool->lru_ticks[i] < oldest_tick) {
                oldest_tick = pool->lru_ticks[i];
                target_slot = i;
            }
        }
        /* Close evicted handle */
        if (pool->open_handles[target_slot]) {
            sqlite3_close_v2(pool->open_handles[target_slot]);
            pool->open_handles[target_slot] = NULL;
            pool->active_ids[target_slot][0] = '\0';
            pool->count--;
        }
    }

    char db_path[CBM_PATH_MAX];
    if (make_horizon_path(pool, horizon_id, db_path, sizeof(db_path)) != 0) {
        *out_db = NULL;
        return -1;
    }

    struct stat st;
    if (stat(db_path, &st) != 0 || st.st_size == 0) {
        *out_db = NULL;
        return -1; /* Horizon file does not exist or is empty; fail fast */
    }

    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL);
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close_v2(db);
        *out_db = NULL;
        return -1;
    }

    sqlite3_exec(db, "PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL;", NULL, NULL, NULL);
    sqlite3_exec(db, HORIZON_DDL, NULL, NULL, NULL);

    pool->open_handles[target_slot] = db;
    snprintf(pool->active_ids[target_slot], sizeof(pool->active_ids[target_slot]), "%s", horizon_id);
    pool->lru_ticks[target_slot] = pool->current_tick;
    pool->count++;

    *out_db = db;
    return 0;
}

int cbm_horizon_set_status(HorizonConnectionPool *pool, const char *horizon_id, HorizonStatus status) {
    if (!pool || !is_valid_horizon_id(horizon_id)) return -1;
    sqlite3 *db = NULL;
    int rc = cbm_horizon_pool_get(pool, horizon_id, &db);
    if (rc != 0 || !db) return -1;

    const char *status_str = "ACTIVE";
    if (status == HORIZON_PROMOTED) status_str = "PROMOTED";
    else if (status == HORIZON_DISCARDED) status_str = "DISCARDED";

    sqlite3_stmt *stmt = NULL;
    const char *upsert_sql =
        "INSERT INTO horizon_metadata (horizon_id, client_pid, status, created_at, last_heartbeat) "
        "VALUES (?, 0, ?, strftime('%s','now'), strftime('%s','now')) "
        "ON CONFLICT(horizon_id) DO UPDATE SET status = excluded.status, last_heartbeat = excluded.last_heartbeat;";
    rc = sqlite3_prepare_v2(db, upsert_sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, horizon_id, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, status_str, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        return 0;
    }
    return -1;
}

int cbm_promote_horizon_state(HorizonConnectionPool *pool, const char *horizon_id) {
    return cbm_horizon_set_status(pool, horizon_id, HORIZON_PROMOTED);
}

int cbm_discard_horizon(HorizonConnectionPool *pool, const char *horizon_id) {
    if (!pool || !horizon_id || !is_valid_horizon_id(horizon_id)) return -1;

    /* Update status to DISCARDED */
    cbm_horizon_set_status(pool, horizon_id, HORIZON_DISCARDED);

    /* Close handle if open in pool */
    for (size_t i = 0; i < CBM_MAX_HORIZON_FDS; i++) {
        if (pool->open_handles[i] && strcmp(pool->active_ids[i], horizon_id) == 0) {
            sqlite3_close_v2(pool->open_handles[i]);
            pool->open_handles[i] = NULL;
            pool->active_ids[i][0] = '\0';
            pool->lru_ticks[i] = 0;
            pool->count--;
            break;
        }
    }

    char db_path[CBM_PATH_MAX];
    if (make_horizon_path(pool, horizon_id, db_path, sizeof(db_path)) != 0) {
        return -1;
    }
    cbm_unlink(db_path);

    char wal_path[CBM_PATH_MAX + 8];
    snprintf(wal_path, sizeof(wal_path), "%s-wal", db_path);
    cbm_unlink(wal_path);

    char shm_path[CBM_PATH_MAX + 8];
    snprintf(shm_path, sizeof(shm_path), "%s-shm", db_path);
    cbm_unlink(shm_path);

    return 0;
}
