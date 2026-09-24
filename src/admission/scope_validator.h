#ifndef CBM_SCOPE_VALIDATOR_H
#define CBM_SCOPE_VALIDATOR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(__has_include)
  #if __has_include(<sqlite3.h>)
    #include <sqlite3.h>
  #elif __has_include("sqlite3.h")
    #include "sqlite3.h"
  #endif
#else
  #include <sqlite3.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t isolated_nodes_count;
    char **isolated_nodes;
    size_t unresolved_deps_count;
    char **unresolved_deps;
} ScopeValidationReport;

void cbm_scope_validation_report_free(ScopeValidationReport *report);

int cbm_validate_scope_horizon(sqlite3 *hdb,
                               sqlite3 *base_db,
                               bool strict_connectivity,
                               ScopeValidationReport *report,
                               char *err_buf,
                               size_t err_sz);

#ifdef __cplusplus
}
#endif

#endif /* CBM_SCOPE_VALIDATOR_H */
