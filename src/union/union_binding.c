/*
 * union_binding.c — Binding Claims (DEVE/PODE) & Deviation Ledger (Scope D02).
 */
#include "union_binding.h"
#include "../foundation/log.h"
#include "../foundation/compat_fs.h"
#include "../foundation/platform.h"
#include <yyjson/yyjson.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

void cbm_binding_ledger_init(CbmBindingLedger *ledger) {
    if (!ledger) return;
    memset(ledger, 0, sizeof(*ledger));
    ledger->storage_ready = true;
}

static bool binding_save(const CbmBindingLedger *ledger) {
    if (!ledger || !ledger->storage_path[0]) return true;
    char tmp[CBM_BINDING_STORE_PATH_MAX + 8];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmp", ledger->storage_path);
    if (n < 0 || (size_t)n >= sizeof(tmp)) return false;
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (!doc) return false;
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_val *bindings = yyjson_mut_arr(doc);
    yyjson_mut_val *deviations = yyjson_mut_arr(doc);
    yyjson_mut_doc_set_root(doc, root);
    bool ok = root && bindings && deviations;
    for (size_t i = 0; ok && i < ledger->binding_count; i++) {
        const CbmBindingClaim *b = &ledger->bindings[i];
        yyjson_mut_val *o = yyjson_mut_obj(doc);
        ok = o && yyjson_mut_obj_add_strcpy(doc,o,"claim_id",b->claim_id) &&
            yyjson_mut_obj_add_strcpy(doc,o,"theme_id",b->theme_id) &&
            yyjson_mut_obj_add_strcpy(doc,o,"pinned_version",b->pinned_version) &&
            yyjson_mut_obj_add_int(doc,o,"mode",b->mode) &&
            yyjson_mut_obj_add_strcpy(doc,o,"binding_scope",b->binding_scope) &&
            yyjson_mut_obj_add_strcpy(doc,o,"validated_by",b->validated_by) &&
            yyjson_mut_obj_add_int(doc,o,"status",b->status) &&
            yyjson_mut_obj_add_uint(doc,o,"created_at",b->created_at) &&
            yyjson_mut_obj_add_uint(doc,o,"updated_at",b->updated_at) &&
            yyjson_mut_arr_add_val(bindings,o);
    }
    for (size_t i = 0; ok && i < ledger->deviation_count; i++) {
        const CbmDeviationClaim *d = &ledger->deviations[i];
        yyjson_mut_val *o = yyjson_mut_obj(doc);
        ok = o && yyjson_mut_obj_add_strcpy(doc,o,"claim_id",d->claim_id) &&
            yyjson_mut_obj_add_strcpy(doc,o,"theme_id",d->theme_id) &&
            yyjson_mut_obj_add_strcpy(doc,o,"theme_rule_node_ref",d->theme_rule_node_ref) &&
            yyjson_mut_obj_add_strcpy(doc,o,"reason",d->reason) &&
            yyjson_mut_obj_add_strcpy(doc,o,"affected_scope",d->affected_scope) &&
            yyjson_mut_obj_add_strcpy(doc,o,"validated_by",d->validated_by) &&
            yyjson_mut_obj_add_int(doc,o,"status",d->status) &&
            yyjson_mut_obj_add_uint(doc,o,"created_at",d->created_at) &&
            yyjson_mut_arr_add_val(deviations,o);
    }
    ok = ok && yyjson_mut_obj_add_val(doc,root,"bindings",bindings) &&
         yyjson_mut_obj_add_val(doc,root,"deviations",deviations) && yyjson_mut_write_file(tmp,doc,0,NULL,NULL);
    yyjson_mut_doc_free(doc);
    if (!ok) { cbm_unlink(tmp); return false; }
    if (cbm_rename_replace(tmp, ledger->storage_path) != 0) { cbm_unlink(tmp); return false; }
    return true;
}

static void binding_read_string(char *dst, size_t cap, yyjson_val *obj, const char *key) {
    yyjson_val *v = yyjson_obj_get(obj,key);
    if (v && yyjson_is_str(v)) snprintf(dst,cap,"%s",yyjson_get_str(v));
}

bool cbm_binding_ledger_open(CbmBindingLedger *ledger, const char *path) {
    if (!ledger || !path || !path[0] || strlen(path) >= sizeof(ledger->storage_path)) return false;
    cbm_binding_ledger_init(ledger);
    snprintf(ledger->storage_path,sizeof(ledger->storage_path),"%s",path);
    ledger->storage_ready = false;
    if (!cbm_file_exists(path)) { ledger->storage_ready = true; return true; }
    yyjson_doc *doc = yyjson_read_file(path,0,NULL,NULL);
    yyjson_val *root = doc ? yyjson_doc_get_root(doc) : NULL;
    yyjson_val *ba = root ? yyjson_obj_get(root,"bindings") : NULL;
    yyjson_val *da = root ? yyjson_obj_get(root,"deviations") : NULL;
    if (!doc || !yyjson_is_arr(ba) || !yyjson_is_arr(da) ||
        yyjson_arr_size(ba) > CBM_BINDING_CAP || yyjson_arr_size(da) > CBM_DEVIATION_CAP) {
        if (doc) yyjson_doc_free(doc);
        return false;
    }
    size_t idx,max; yyjson_val *v;
    yyjson_arr_foreach(ba,idx,max,v) {
        CbmBindingClaim *b=&ledger->bindings[ledger->binding_count++];
        binding_read_string(b->claim_id,sizeof(b->claim_id),v,"claim_id");
        binding_read_string(b->theme_id,sizeof(b->theme_id),v,"theme_id");
        binding_read_string(b->pinned_version,sizeof(b->pinned_version),v,"pinned_version");
        binding_read_string(b->binding_scope,sizeof(b->binding_scope),v,"binding_scope");
        binding_read_string(b->validated_by,sizeof(b->validated_by),v,"validated_by");
        yyjson_val *x=yyjson_obj_get(v,"mode"); if(x&&yyjson_is_int(x)) b->mode=(CbmBindingMode)yyjson_get_int(x);
        x=yyjson_obj_get(v,"status"); if(x&&yyjson_is_int(x)) b->status=(CbmBindingStatus)yyjson_get_int(x);
        x=yyjson_obj_get(v,"created_at"); if(x&&yyjson_is_uint(x)) b->created_at=yyjson_get_uint(x);
        x=yyjson_obj_get(v,"updated_at"); if(x&&yyjson_is_uint(x)) b->updated_at=yyjson_get_uint(x);
        if(!b->theme_id[0]||!b->pinned_version[0]) { yyjson_doc_free(doc); return false; }
    }
    yyjson_arr_foreach(da,idx,max,v) {
        CbmDeviationClaim *d=&ledger->deviations[ledger->deviation_count++];
        binding_read_string(d->claim_id,sizeof(d->claim_id),v,"claim_id");
        binding_read_string(d->theme_id,sizeof(d->theme_id),v,"theme_id");
        binding_read_string(d->theme_rule_node_ref,sizeof(d->theme_rule_node_ref),v,"theme_rule_node_ref");
        binding_read_string(d->reason,sizeof(d->reason),v,"reason");
        binding_read_string(d->affected_scope,sizeof(d->affected_scope),v,"affected_scope");
        binding_read_string(d->validated_by,sizeof(d->validated_by),v,"validated_by");
        yyjson_val *x=yyjson_obj_get(v,"status"); if(x&&yyjson_is_int(x)) d->status=(CbmDeviationStatus)yyjson_get_int(x);
        x=yyjson_obj_get(v,"created_at"); if(x&&yyjson_is_uint(x)) d->created_at=yyjson_get_uint(x);
        if(!d->theme_id[0]||!d->reason[0]) { yyjson_doc_free(doc); return false; }
    }
    yyjson_doc_free(doc);
    ledger->storage_ready=true;
    return true;
}

CbmRefusalCode cbm_binding_validate_and_admit(CbmBindingLedger *ledger,
                                              const CbmBindingClaim *binding,
                                              char *err_reason,
                                              size_t err_len) {
    if (!ledger || !binding) {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "null ledger or binding");
        return CBM_REFUSAL_CLAIM_INVALID;
    }
    if (!ledger->storage_ready) return CBM_REFUSAL_BINDING_PERSISTENCE_FAILED;

    if (binding->theme_id[0] == '\0') {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "missing mandatory field: theme_id");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    if (binding->pinned_version[0] == '\0') {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "missing mandatory field: pinned_version");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    /* Agent cannot self-validate a normative or consulted binding */
    if (binding->validated_by[0] != '\0') {
        if (strstr(binding->validated_by, "agent") != NULL) {
            if (err_reason && err_len > 0) {
                snprintf(err_reason, err_len, "theme bindings require operator validation; self-validation by agent is forbidden");
            }
            cbm_refusal_emit(CBM_REFUSAL_BINDING_SELF_VALIDATED, "binding_ledger", "agent attempted self-validation");
            return CBM_REFUSAL_BINDING_SELF_VALIDATED;
        }
    } else if (binding->mode == CBM_BINDING_NORMATIVE) {
        /* Normative (DEVE) binding mandates operator validation */
        if (err_reason && err_len > 0) {
            snprintf(err_reason, err_len, "normative binding mandates operator validation");
        }
        cbm_refusal_emit(CBM_REFUSAL_BINDING_SELF_VALIDATED, "binding_ledger", "normative binding missing operator validation");
        return CBM_REFUSAL_BINDING_SELF_VALIDATED;
    }

    CbmBindingLedger previous = *ledger;
    /* Update if existing theme binding */
    for (size_t i = 0; i < ledger->binding_count; i++) {
        if (strcmp(ledger->bindings[i].theme_id, binding->theme_id) == 0) {
            ledger->bindings[i] = *binding;
            ledger->bindings[i].updated_at = (uint64_t)time(NULL);
            if (!binding_save(ledger)) { *ledger = previous; return CBM_REFUSAL_BINDING_PERSISTENCE_FAILED; }
            return CBM_REFUSAL_OK;
        }
    }

    if (ledger->binding_count >= CBM_BINDING_CAP) {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "binding ledger capacity exceeded");
        return CBM_REFUSAL_SCOPE_EXCEEDED;
    }

    ledger->bindings[ledger->binding_count] = *binding;
    ledger->bindings[ledger->binding_count].created_at = (uint64_t)time(NULL);
    ledger->bindings[ledger->binding_count].updated_at = ledger->bindings[ledger->binding_count].created_at;
    ledger->binding_count++;

    if (!binding_save(ledger)) { *ledger = previous; return CBM_REFUSAL_BINDING_PERSISTENCE_FAILED; }

    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_binding_lookup(const CbmBindingLedger *ledger,
                                 const char *theme_id,
                                 CbmBindingClaim *out_binding) {
    if (!ledger || !theme_id) return CBM_REFUSAL_THEME_UNKNOWN;

    for (size_t i = 0; i < ledger->binding_count; i++) {
        if (strcmp(ledger->bindings[i].theme_id, theme_id) == 0) {
            if (out_binding) *out_binding = ledger->bindings[i];
            return CBM_REFUSAL_OK;
        }
    }
    return CBM_REFUSAL_THEME_UNKNOWN;
}

CbmRefusalCode cbm_deviation_admit(CbmBindingLedger *ledger,
                                   const CbmDeviationClaim *deviation,
                                   char *err_reason,
                                   size_t err_len) {
    if (!ledger || !deviation) {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "null ledger or deviation");
        return CBM_REFUSAL_CLAIM_INVALID;
    }
    if (!ledger->storage_ready) return CBM_REFUSAL_BINDING_PERSISTENCE_FAILED;

    if (deviation->theme_id[0] == '\0') {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "missing mandatory field: theme_id");
        return CBM_REFUSAL_CLAIM_INVALID;
    }
    if (deviation->theme_rule_node_ref[0] == '\0') {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "missing mandatory field: theme_rule_node_ref");
        return CBM_REFUSAL_CLAIM_INVALID;
    }
    if (deviation->reason[0] == '\0') {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "missing mandatory field: reason");
        cbm_refusal_emit(CBM_REFUSAL_CLAIM_INVALID, "deviation_ledger", "missing mandatory field: reason");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    if (ledger->deviation_count >= CBM_DEVIATION_CAP) {
        if (err_reason && err_len > 0) snprintf(err_reason, err_len, "deviation ledger capacity exceeded");
        return CBM_REFUSAL_SCOPE_EXCEEDED;
    }

    CbmBindingLedger previous = *ledger;
    ledger->deviations[ledger->deviation_count] = *deviation;
    ledger->deviations[ledger->deviation_count].created_at = (uint64_t)time(NULL);
    ledger->deviation_count++;

    if (!binding_save(ledger)) { *ledger = previous; return CBM_REFUSAL_BINDING_PERSISTENCE_FAILED; }

    return CBM_REFUSAL_OK;
}

CbmRefusalCode cbm_deviation_query_active(const CbmBindingLedger *ledger,
                                          const char *theme_id,
                                          CbmDeviationClaim *out_devs,
                                          size_t max_devs,
                                          size_t *out_count) {
    if (!ledger || !out_count) return CBM_REFUSAL_CLAIM_INVALID;

    size_t matched = 0;
    for (size_t i = 0; i < ledger->deviation_count && matched < max_devs; i++) {
        if (ledger->deviations[i].status == CBM_DEVIATION_ACTIVE) {
            if (!theme_id || strcmp(ledger->deviations[i].theme_id, theme_id) == 0) {
                if (out_devs) out_devs[matched] = ledger->deviations[i];
                matched++;
            }
        }
    }
    *out_count = matched;
    return CBM_REFUSAL_OK;
}
