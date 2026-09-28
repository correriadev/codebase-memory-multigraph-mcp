/*
 * union_handler.h — MCP tool handlers for Union Workflow (Track W: W01-W05, W07).
 */
#ifndef CBM_UNION_HANDLER_H
#define CBM_UNION_HANDLER_H

#include "mcp.h"
#include "mcp_internal.h"
#include "../union/union_session.h"
#include "../union/union_contract.h"
#include "../union/union_gateway.h"
#include "../union/union_theme_registry.h"
#include "../union/union_binding.h"
#include "../union/union_routing.h"
#include "../union/union_founding.h"
#include "../union/union_contest.h"
#include "../union/union_sweep.h"
#include "../union/union_trace.h"

#ifdef __cplusplus
extern "C" {
#endif

char *handle_union_session_open(cbm_mcp_server_t *srv, const char *args_json);
char *handle_union_session_get(cbm_mcp_server_t *srv, const char *args_json);
char *handle_union_session_close(cbm_mcp_server_t *srv, const char *args_json);
char *handle_union_record_action(cbm_mcp_server_t *srv, const char *args_json);
char *handle_classify_activity(cbm_mcp_server_t *srv, const char *args_json);
char *handle_validate_provenance(cbm_mcp_server_t *srv, const char *args_json);
char *handle_theme_lookup(cbm_mcp_server_t *srv, const char *args_json);
char *handle_theme_register(cbm_mcp_server_t *srv, const char *args_json);
char *handle_binding_claim(cbm_mcp_server_t *srv, const char *args_json);
char *handle_founding_propose(cbm_mcp_server_t *srv, const char *args_json);
char *handle_founding_decide(cbm_mcp_server_t *srv, const char *args_json);
char *handle_contest_verify(cbm_mcp_server_t *srv, const char *args_json);
char *handle_union_claim_capture(cbm_mcp_server_t *srv, const char *args_json);
char *handle_union_claim_resolve(cbm_mcp_server_t *srv, const char *args_json);

CbmSessionSweepContext *cbm_mcp_get_session_sweep_context(const char *horizon_id);

void cbm_union_reset_proposal_store_for_test(void);
void cbm_union_reload_proposals_for_test(void);

#ifdef __cplusplus
}
#endif

#endif /* CBM_UNION_HANDLER_H */
