#ifndef CBM_HARNESS_HANDLERS_H
#define CBM_HARNESS_HANDLERS_H

#include "mcp/mcp.h"
#include "mcp/mcp_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

char *handle_extract_harness_bundle(cbm_mcp_server_t *srv, const char *args_json);
char *handle_evaluate_fast_path(cbm_mcp_server_t *srv, const char *args_json);
char *handle_record_admission_manifest(cbm_mcp_server_t *srv, const char *args_json);
char *handle_tradition_propose(cbm_mcp_server_t *srv, const char *args_json);

#ifdef __cplusplus
}
#endif

#endif /* CBM_HARNESS_HANDLERS_H */
