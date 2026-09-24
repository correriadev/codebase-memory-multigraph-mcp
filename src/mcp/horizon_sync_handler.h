#ifndef CBM_HORIZON_SYNC_HANDLER_H
#define CBM_HORIZON_SYNC_HANDLER_H

#include "mcp.h"
#include "mcp_internal.h"
#include "../core/horizon_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

char *handle_sync_horizon_spec(cbm_mcp_server_t *srv, const char *args_json, HorizonConnectionPool *pool);
char *handle_validate_scope_horizon(cbm_mcp_server_t *srv, const char *args_json, HorizonConnectionPool *pool);

#ifdef __cplusplus
}
#endif

#endif /* CBM_HORIZON_SYNC_HANDLER_H */
