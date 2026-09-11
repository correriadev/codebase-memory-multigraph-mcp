#ifndef CBM_HORIZON_REAPER_H
#define CBM_HORIZON_REAPER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CBM_HORIZON_TTL_SECONDS 3600

typedef struct {
    uint64_t reaped_count;
    uint64_t retained_count;
    uint64_t bytes_freed;
} HorizonReaperStats;

/* Check if a client PID is currently alive on the host OS */
bool cbm_is_pid_alive(uint32_t pid);

/* Reaps orphan horizon files with dead client PID and age > TTL */
int cbm_reap_orphan_horizons(const char *horizons_dir, HorizonReaperStats *out_stats);

#endif /* CBM_HORIZON_REAPER_H */
