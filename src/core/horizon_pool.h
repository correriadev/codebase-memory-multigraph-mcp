#ifndef CBM_HORIZON_POOL_H
#define CBM_HORIZON_POOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(__has_include)
  #if __has_include(<sqlite3.h>)
    #include <sqlite3.h>
  #elif __has_include("sqlite3.h")
    #include "sqlite3.h"
  #endif
#else
  #include <sqlite3.h>
#endif

#define CBM_MAX_HORIZON_FDS 16
#define CBM_HORIZON_ID_MAX 64
#define CBM_PATH_MAX 1024

typedef enum {
    HORIZON_ACTIVE = 0,
    HORIZON_PROMOTED = 1,
    HORIZON_DISCARDED = 2
} HorizonStatus;

typedef struct {
    char id[CBM_HORIZON_ID_MAX];
    uint32_t client_pid;
    HorizonStatus status;
    uint64_t created_at;
    uint64_t last_heartbeat;
} HorizonAggregate;

typedef struct HorizonConnectionPool {
    sqlite3 *open_handles[CBM_MAX_HORIZON_FDS];
    char active_ids[CBM_MAX_HORIZON_FDS][CBM_HORIZON_ID_MAX];
    uint64_t lru_ticks[CBM_MAX_HORIZON_FDS];
    size_t count;
    uint64_t current_tick;
    char base_dir[CBM_PATH_MAX];
} HorizonConnectionPool;

/* Initialize the connection pool */
int cbm_horizon_pool_init(HorizonConnectionPool *pool, const char *storage_dir);

/* Close all handles in the pool */
void cbm_horizon_pool_close_all(HorizonConnectionPool *pool);

/* Creates a new isolated horizon database with horizon_schema.sql applied */
int cbm_create_horizon(HorizonConnectionPool *pool, uint32_t client_pid, const char *custom_id, char *out_id, size_t out_sz);

/* Transparently fetches or reopens an SQLite handle with LRU eviction */
int cbm_horizon_pool_get(HorizonConnectionPool *pool, const char *horizon_id, sqlite3 **out_db);

/* Transition horizon status */
int cbm_horizon_set_status(HorizonConnectionPool *pool, const char *horizon_id, HorizonStatus status);

/* Discard an ephemeral horizon and clean up files */
int cbm_discard_horizon(HorizonConnectionPool *pool, const char *horizon_id);

/* Promote horizon state */
int cbm_promote_horizon_state(HorizonConnectionPool *pool, const char *horizon_id);

#endif /* CBM_HORIZON_POOL_H */
