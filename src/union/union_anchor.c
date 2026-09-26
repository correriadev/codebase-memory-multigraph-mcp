/*
 * union_anchor.c — Anchor Kinds & Ground Verification (Scope C02).
 */
#include "union_anchor.h"
#include "../foundation/compat.h"
#include "../foundation/compat_fs.h"

#include <stdio.h>
#include <string.h>

uint64_t cbm_anchor_hash_text(const char *text) {
    if (!text) return 0;
    uint64_t hash = 1469598103934665603ULL;
    for (const char *s = text; *s; s++) {
        hash ^= (uint64_t)(unsigned char)*s;
        hash *= 1099511628211ULL;
    }
    return hash == 0 ? 1 : hash;
}

void cbm_log_store_init(CbmLogStore *store) {
    if (!store) return;
    memset(store, 0, sizeof(*store));
}

bool cbm_log_store_append(CbmLogStore *store,
                          const char *session_id,
                          const char *horizon_id,
                          uint64_t event_seq,
                          const char *text) {
    if (!store || !session_id || !text) return false;
    if (store->count >= CBM_LOG_STORE_CAP) return false;

    CbmLogEvent *ev = &store->events[store->count];
    strncpy(ev->session_id, session_id, sizeof(ev->session_id) - 1);
    if (horizon_id) {
        strncpy(ev->horizon_id, horizon_id, sizeof(ev->horizon_id) - 1);
    }
    ev->event_seq = event_seq;
    strncpy(ev->text, text, sizeof(ev->text) - 1);
    ev->content_hash = cbm_anchor_hash_text(text);
    store->count++;
    return true;
}

const CbmLogEvent *cbm_log_store_find(const CbmLogStore *store,
                                      const char *session_id,
                                      uint64_t event_seq) {
    if (!store || !session_id) return NULL;
    for (size_t i = 0; i < store->count; i++) {
        if (store->events[i].event_seq == event_seq &&
            strcmp(store->events[i].session_id, session_id) == 0) {
            return &store->events[i];
        }
    }
    return NULL;
}

void cbm_derivation_store_init(CbmDerivationStore *store) {
    if (!store) return;
    memset(store, 0, sizeof(*store));
}

bool cbm_derivation_store_set(CbmDerivationStore *store,
                              const char *query_ref,
                              uint64_t output_hash) {
    if (!store || !query_ref) return false;

    for (size_t i = 0; i < store->count; i++) {
        if (strcmp(store->entries[i].query_ref, query_ref) == 0) {
            store->entries[i].current_hash = output_hash;
            return true;
        }
    }

    if (store->count < CBM_DERIVATION_STORE_CAP) {
        strncpy(store->entries[store->count].query_ref, query_ref,
                sizeof(store->entries[0].query_ref) - 1);
        store->entries[store->count].current_hash = output_hash;
        store->count++;
        return true;
    }
    return false;
}

CbmRefusalCode cbm_anchor_validate_structure(const CbmAnchor *anchor,
                                            char *out_reason,
                                            size_t reason_sz) {
    if (!anchor || anchor->kind == CBM_ANCHOR_NONE) {
        if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "missing or empty anchor");
        return CBM_REFUSAL_CLAIM_INVALID;
    }

    switch (anchor->kind) {
        case CBM_ANCHOR_FILE_BYTES:
            if (!anchor->file_bytes.file_path[0] ||
                anchor->file_bytes.byte_len == 0 ||
                !anchor->file_bytes.expected_text[0]) {
                if (out_reason && reason_sz > 0) {
                    snprintf(out_reason, reason_sz, "malformed anchor: missing file_path or expected_text");
                }
                return CBM_REFUSAL_CLAIM_INVALID;
            }
            break;
        case CBM_ANCHOR_LOG_REF:
            if (!anchor->log_ref.session_id[0] || anchor->log_ref.event_seq == 0) {
                if (out_reason && reason_sz > 0) {
                    snprintf(out_reason, reason_sz, "malformed anchor: missing session_id or event_seq");
                }
                return CBM_REFUSAL_CLAIM_INVALID;
            }
            break;
        case CBM_ANCHOR_DERIVATION_REF:
            if (!anchor->derivation_ref.generator_query_ref[0]) {
                if (out_reason && reason_sz > 0) {
                    snprintf(out_reason, reason_sz, "malformed anchor: missing generator_query_ref");
                }
                return CBM_REFUSAL_CLAIM_INVALID;
            }
            break;
        default:
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz, "unknown anchor kind");
            }
            return CBM_REFUSAL_CLAIM_INVALID;
    }

    return CBM_REFUSAL_OK;
}

CbmAnchorVerifyStatus cbm_anchor_verify(const CbmAnchor *anchor,
                                        const CbmAnchorEnv *env,
                                        CbmRefusalCode *out_refusal,
                                        char *out_reason,
                                        size_t reason_sz) {
    CbmRefusalCode val = cbm_anchor_validate_structure(anchor, out_reason, reason_sz);
    if (val != CBM_REFUSAL_OK) {
        if (out_refusal) *out_refusal = val;
        return CBM_ANCHOR_VERIFY_MALFORMED;
    }

    if (anchor->kind == CBM_ANCHOR_LOG_REF) {
        if (!env || !env->log_store) {
            if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "no log store in anchor environment");
            if (out_refusal) *out_refusal = CBM_REFUSAL_ANCHOR_NOT_FOUND;
            return CBM_ANCHOR_VERIFY_NOT_FOUND;
        }

        const CbmLogEvent *ev = cbm_log_store_find(env->log_store,
                                                   anchor->log_ref.session_id,
                                                   anchor->log_ref.event_seq);
        if (!ev) {
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz, "event_seq %llu not found in session %s",
                         (unsigned long long)anchor->log_ref.event_seq,
                         anchor->log_ref.session_id);
            }
            if (out_refusal) *out_refusal = CBM_REFUSAL_ANCHOR_NOT_FOUND;
            return CBM_ANCHOR_VERIFY_NOT_FOUND;
        }

        if (anchor->log_ref.content_hash != 0 &&
            ev->content_hash != anchor->log_ref.content_hash) {
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz, "content hash mismatch: expected %llu got %llu",
                         (unsigned long long)anchor->log_ref.content_hash,
                         (unsigned long long)ev->content_hash);
            }
            if (out_refusal) *out_refusal = CBM_REFUSAL_ANCHOR_NOT_FOUND;
            return CBM_ANCHOR_VERIFY_NOT_FOUND;
        }

        if (out_refusal) *out_refusal = CBM_REFUSAL_OK;
        return CBM_ANCHOR_VERIFY_OK;
    }

    if (anchor->kind == CBM_ANCHOR_FILE_BYTES) {
        char buf[512] = {0};
        bool ok = false;
        if (env && env->file_reader) {
            ok = env->file_reader(anchor->file_bytes.file_path,
                                  anchor->file_bytes.byte_start,
                                  anchor->file_bytes.byte_len,
                                  buf, sizeof(buf),
                                  env->file_reader_ctx);
        } else {
            FILE *f = cbm_fopen(anchor->file_bytes.file_path, "rb");
            if (f) {
                if (fseek(f, (long)anchor->file_bytes.byte_start, SEEK_SET) == 0) {
                    size_t read_bytes = fread(buf, 1, anchor->file_bytes.byte_len, f);
                    if (read_bytes == anchor->file_bytes.byte_len) {
                        buf[read_bytes] = '\0';
                        ok = true;
                    }
                }
                fclose(f);
            }
        }

        if (!ok) {
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz, "unable to read file bytes from %s",
                         anchor->file_bytes.file_path);
            }
            if (out_refusal) *out_refusal = CBM_REFUSAL_ANCHOR_NOT_FOUND;
            return CBM_ANCHOR_VERIFY_NOT_FOUND;
        }

        if (strcmp(buf, anchor->file_bytes.expected_text) != 0) {
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz, "byte mismatch in file_bytes anchor: expected '%s' got '%s'",
                         anchor->file_bytes.expected_text, buf);
            }
            if (out_refusal) *out_refusal = CBM_REFUSAL_ANCHOR_NOT_FOUND;
            return CBM_ANCHOR_VERIFY_DRIFTED;
        }

        if (out_refusal) *out_refusal = CBM_REFUSAL_OK;
        return CBM_ANCHOR_VERIFY_OK;
    }

    if (anchor->kind == CBM_ANCHOR_DERIVATION_REF) {
        if (!env || !env->derivation_store) {
            if (out_reason && reason_sz > 0) snprintf(out_reason, reason_sz, "no derivation store in environment");
            if (out_refusal) *out_refusal = CBM_REFUSAL_ANCHOR_NOT_FOUND;
            return CBM_ANCHOR_VERIFY_NOT_FOUND;
        }

        bool found = false;
        uint64_t current_hash = 0;
        for (size_t i = 0; i < env->derivation_store->count; i++) {
            if (strcmp(env->derivation_store->entries[i].query_ref,
                       anchor->derivation_ref.generator_query_ref) == 0) {
                found = true;
                current_hash = env->derivation_store->entries[i].current_hash;
                break;
            }
        }

        if (!found) {
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz, "generator query %s not found in derivation store",
                         anchor->derivation_ref.generator_query_ref);
            }
            if (out_refusal) *out_refusal = CBM_REFUSAL_ANCHOR_NOT_FOUND;
            return CBM_ANCHOR_VERIFY_NOT_FOUND;
        }

        if (current_hash != anchor->derivation_ref.output_hash) {
            if (out_reason && reason_sz > 0) {
                snprintf(out_reason, reason_sz,
                         "derivation output changed: expected %llu got %llu",
                         (unsigned long long)anchor->derivation_ref.output_hash,
                         (unsigned long long)current_hash);
            }
            if (out_refusal) *out_refusal = CBM_REFUSAL_ANCHOR_NOT_FOUND;
            return CBM_ANCHOR_VERIFY_INVALID_DERIVATION;
        }

        if (out_refusal) *out_refusal = CBM_REFUSAL_OK;
        return CBM_ANCHOR_VERIFY_OK;
    }

    if (out_refusal) *out_refusal = CBM_REFUSAL_CLAIM_INVALID;
    return CBM_ANCHOR_VERIFY_MALFORMED;
}
