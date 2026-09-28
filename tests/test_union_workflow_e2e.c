/*
 * test_union_workflow_e2e.c — End-to-end integration suite for Union Workflow (Track W: W01-W05, W07, W09).
 */
#include "test_framework.h"
#include "../src/mcp/union_handler.h"
#include "../src/mcp/mcp.h"
#include "../src/mcp/mcp_internal.h"
#include "../src/admission/admission_gate.h"
#include <yyjson/yyjson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static yyjson_val *get_payload(yyjson_doc *doc) {
    if (!doc) return NULL;
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (!root) return NULL;
    yyjson_val *sc = yyjson_obj_get(root, "structuredContent");
    return sc ? sc : root;
}

static bool get_is_error(yyjson_doc *doc) {
    if (!doc) return false;
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *v_err = yyjson_obj_get(root, "isError");
    if (v_err && yyjson_get_bool(v_err)) return true;
    yyjson_val *sc = yyjson_obj_get(root, "structuredContent");
    if (sc) {
        yyjson_val *v_err_sc = yyjson_obj_get(sc, "isError");
        if (v_err_sc && yyjson_get_bool(v_err_sc)) return true;
    }
    return false;
}

TEST(test_w01_session_lifecycle) {
    cbm_mcp_server_t *srv = cbm_mcp_server_new(NULL);
    ASSERT(srv != NULL);

    /* 1. Open session without contract -> restricted mode */
    const char *open_args = "{\"identity\":\"developer\",\"contract_id\":\"unregistered_contract\"}";
    char *open_res = handle_union_session_open(srv, open_args);
    ASSERT(open_res != NULL);

    yyjson_doc *doc = yyjson_read(open_res, strlen(open_res), 0);
    ASSERT(doc != NULL);
    yyjson_val *payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_ok = yyjson_obj_get(payload, "success");
    ASSERT(v_ok && yyjson_get_bool(v_ok) == true);

    yyjson_val *v_hid = yyjson_obj_get(payload, "horizon_id");
    ASSERT(v_hid && yyjson_is_str(v_hid));
    char horizon_id[64];
    strncpy(horizon_id, yyjson_get_str(v_hid), sizeof(horizon_id) - 1);
    horizon_id[sizeof(horizon_id) - 1] = '\0';

    yyjson_val *v_rest = yyjson_obj_get(payload, "restricted");
    ASSERT(v_rest && yyjson_get_bool(v_rest) == true);
    yyjson_doc_free(doc);
    free(open_res);

    /* 2. Get session */
    char get_args[128];
    snprintf(get_args, sizeof(get_args), "{\"horizon_id\":\"%s\"}", horizon_id);
    char *get_res = handle_union_session_get(srv, get_args);
    ASSERT(get_res != NULL);

    doc = yyjson_read(get_res, strlen(get_res), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    v_ok = yyjson_obj_get(payload, "success");
    ASSERT(v_ok && yyjson_get_bool(v_ok) == true);
    yyjson_val *v_act = yyjson_obj_get(payload, "actions");
    ASSERT(v_act && yyjson_get_int(v_act) == 0);
    yyjson_doc_free(doc);
    free(get_res);

    /* 3. Close session */
    char close_args[128];
    snprintf(close_args, sizeof(close_args), "{\"horizon_id\":\"%s\",\"reason\":\"NORMAL\"}", horizon_id);
    char *close_res = handle_union_session_close(srv, close_args);
    ASSERT(close_res != NULL);

    doc = yyjson_read(close_res, strlen(close_res), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    v_ok = yyjson_obj_get(payload, "success");
    ASSERT(v_ok && yyjson_get_bool(v_ok) == true);
    yyjson_doc_free(doc);
    free(close_res);

    /* 4. Query after close -> not found (isError == true) */
    char *get_closed = handle_union_session_get(srv, get_args);
    ASSERT(get_closed != NULL);
    doc = yyjson_read(get_closed, strlen(get_closed), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    yyjson_doc_free(doc);
    free(get_closed);

    cbm_mcp_server_free(srv);
    PASS();
}

TEST(test_w02_action_gateway_restricted_mode) {
    cbm_mcp_server_t *srv = cbm_mcp_server_new(NULL);
    ASSERT(srv != NULL);

    /* Open session in restricted mode */
    const char *open_args = "{\"identity\":\"junior_dev\"}";
    char *open_res = handle_union_session_open(srv, open_args);
    ASSERT(open_res != NULL);
    yyjson_doc *doc = yyjson_read(open_res, strlen(open_res), 0);
    ASSERT(doc != NULL);
    yyjson_val *payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_hid = yyjson_obj_get(payload, "horizon_id");
    ASSERT(v_hid && yyjson_is_str(v_hid));
    char h_id[64];
    strncpy(h_id, yyjson_get_str(v_hid), sizeof(h_id) - 1);
    h_id[sizeof(h_id) - 1] = '\0';
    yyjson_doc_free(doc);
    free(open_res);

    /* 1. IDEMPOTENT action allowed */
    char act_args[256];
    snprintf(act_args, sizeof(act_args),
             "{\"horizon_id\":\"%s\",\"action_name\":\"read_query\",\"effect_class\":\"IDEMPOTENT\"}", h_id);
    char *act_res = handle_union_record_action(srv, act_args);
    ASSERT(act_res != NULL);
    doc = yyjson_read(act_res, strlen(act_res), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_auth = yyjson_obj_get(payload, "authorized");
    ASSERT(v_auth && yyjson_get_bool(v_auth) == true);
    yyjson_doc_free(doc);
    free(act_res);

    /* 2. IRREVERSIBLE action blocked by restricted mode */
    snprintf(act_args, sizeof(act_args),
             "{\"horizon_id\":\"%s\",\"action_name\":\"drop_table\",\"effect_class\":\"IRREVERSIBLE\"}", h_id);
    char *block_res = handle_union_record_action(srv, act_args);
    ASSERT(block_res != NULL);
    doc = yyjson_read(block_res, strlen(block_res), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_code = yyjson_obj_get(payload, "code");
    ASSERT(v_code && yyjson_is_str(v_code));
    ASSERT_STR_EQ(yyjson_get_str(v_code), "RESTRICTED_MODE_BLOCKED");
    yyjson_doc_free(doc);
    free(block_res);

    /* 3. Check session refusal count incremented */
    char get_args[128];
    snprintf(get_args, sizeof(get_args), "{\"horizon_id\":\"%s\"}", h_id);
    char *get_res = handle_union_session_get(srv, get_args);
    ASSERT(get_res != NULL);
    doc = yyjson_read(get_res, strlen(get_res), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_ref = yyjson_obj_get(payload, "refusals");
    ASSERT(v_ref && yyjson_get_int(v_ref) == 1);
    yyjson_val *v_act = yyjson_obj_get(payload, "actions");
    ASSERT(v_act && yyjson_get_int(v_act) == 1);
    yyjson_doc_free(doc);
    free(get_res);

    cbm_mcp_server_free(srv);
    PASS();
}

TEST(test_w03_promote_blocked_by_active_contest) {
    cbm_mcp_server_t *srv = cbm_mcp_server_new(NULL);
    ASSERT(srv != NULL);

    /* 1. Open session */
    const char *open_args = "{\"identity\":\"architect\"}";
    char *open_res = handle_union_session_open(srv, open_args);
    ASSERT(open_res != NULL);
    yyjson_doc *doc = yyjson_read(open_res, strlen(open_res), 0);
    ASSERT(doc != NULL);
    yyjson_val *payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_hid = yyjson_obj_get(payload, "horizon_id");
    ASSERT(v_hid && yyjson_is_str(v_hid));
    char h_id[64];
    strncpy(h_id, yyjson_get_str(v_hid), sizeof(h_id) - 1);
    h_id[sizeof(h_id) - 1] = '\0';
    yyjson_doc_free(doc);
    free(open_res);

    /* 2. Submit blocking contest against horizon via W07 contest_verify */
    char contest_args[256];
    snprintf(contest_args, sizeof(contest_args),
             "{\"target_ref\":\"%s\",\"severity\":\"BLOCKING\",\"evidence\":[\"invariant_broken_in_auth\"]}", h_id);
    char *contest_res = handle_contest_verify(srv, contest_args);
    ASSERT(contest_res != NULL);
    doc = yyjson_read(contest_res, strlen(contest_res), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_c = yyjson_obj_get(payload, "contested");
    ASSERT(v_c && yyjson_get_bool(v_c) == true);
    yyjson_doc_free(doc);
    free(contest_res);

    /* 3. Attempt promotion -> must be rejected by session gate due to active blocking contest */
    char prom_args[256];
    snprintf(prom_args, sizeof(prom_args), "{\"horizon_id\":\"%s\"}", h_id);
    char *prom_res = handle_promote_horizon(srv, prom_args, cbm_mcp_server_horizon_pool(srv), NULL);
    ASSERT(prom_res != NULL);
    doc = yyjson_read(prom_res, strlen(prom_res), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_code = yyjson_obj_get(payload, "code");
    ASSERT(v_code && yyjson_is_str(v_code));
    ASSERT_STR_EQ(yyjson_get_str(v_code), "CONTEST_BLOCKED");
    yyjson_doc_free(doc);
    free(prom_res);

    cbm_mcp_server_free(srv);
    PASS();
}

TEST(test_w04_routing_and_provenance_validation) {
    cbm_mcp_server_t *srv = cbm_mcp_server_new(NULL);
    ASSERT(srv != NULL);

    /* 1. CONSULTATIVE classification */
    char *cls1 = handle_classify_activity(srv, "{\"intent\":\"explain module flow for order handler\"}");
    ASSERT(cls1 != NULL);
    yyjson_doc *doc = yyjson_read(cls1, strlen(cls1), 0);
    ASSERT(doc != NULL);
    yyjson_val *payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_cls1 = yyjson_obj_get(payload, "activity_class");
    ASSERT(v_cls1 && yyjson_is_str(v_cls1));
    ASSERT_STR_EQ(yyjson_get_str(v_cls1), "CONSULTATIVE");
    yyjson_doc_free(doc);
    free(cls1);

    /* 2. SPECIALTY classification */
    char *cls2 = handle_classify_activity(srv, "{\"intent\":\"scaffold domain aggregate under clean-arch\"}");
    ASSERT(cls2 != NULL);
    doc = yyjson_read(cls2, strlen(cls2), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_cls2 = yyjson_obj_get(payload, "activity_class");
    ASSERT(v_cls2 && yyjson_is_str(v_cls2));
    ASSERT_STR_EQ(yyjson_get_str(v_cls2), "SPECIALTY");
    yyjson_doc_free(doc);
    free(cls2);

    /* 3. Undeclared provenance refused */
    char *prov1 = handle_validate_provenance(srv, "{}");
    ASSERT(prov1 != NULL);
    doc = yyjson_read(prov1, strlen(prov1), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_code1 = yyjson_obj_get(payload, "code");
    ASSERT(v_code1 && yyjson_is_str(v_code1));
    ASSERT_STR_EQ(yyjson_get_str(v_code1), "PROVENANCE_UNDECLARED");
    yyjson_doc_free(doc);
    free(prov1);

    /* 4. Declared invention accepted */
    char *prov2 = handle_validate_provenance(srv, "{\"declared_invention\":true,\"rationale\":\"novel payment split logic\"}");
    ASSERT(prov2 != NULL);
    doc = yyjson_read(prov2, strlen(prov2), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_val = yyjson_obj_get(payload, "valid");
    ASSERT(v_val && yyjson_get_bool(v_val) == true);
    yyjson_val *v_pcls = yyjson_obj_get(payload, "provenance_class");
    ASSERT(v_pcls && yyjson_is_str(v_pcls));
    ASSERT_STR_EQ(yyjson_get_str(v_pcls), "DECLARED_INVENTION");
    yyjson_doc_free(doc);
    free(prov2);

    cbm_mcp_server_free(srv);
    PASS();
}

TEST(test_w05_territory_and_founding) {
    cbm_mcp_server_t *srv = cbm_mcp_server_new(NULL);
    ASSERT(srv != NULL);

    /* 1. Register theme */
    const char *reg_args = "{\"theme_id\":\"@org/clean-arch\",\"namespace\":\"architecture\",\"curator\":\"lead\",\"version\":\"1.0.0\"}";
    char *reg_res = handle_theme_register(srv, reg_args);
    ASSERT(reg_res != NULL);
    yyjson_doc *doc = yyjson_read(reg_res, strlen(reg_res), 0);
    ASSERT(doc != NULL);
    yyjson_val *payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_reg_ok = yyjson_obj_get(payload, "success");
    ASSERT(v_reg_ok && yyjson_get_bool(v_reg_ok) == true);
    yyjson_doc_free(doc);
    free(reg_res);

    /* 2. Lookup theme */
    char *look_res = handle_theme_lookup(srv, "{\"theme_id\":\"@org/clean-arch\"}");
    ASSERT(look_res != NULL);
    doc = yyjson_read(look_res, strlen(look_res), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_look_ok = yyjson_obj_get(payload, "success");
    ASSERT(v_look_ok && yyjson_get_bool(v_look_ok) == true);
    yyjson_val *v_st = yyjson_obj_get(payload, "status");
    ASSERT(v_st && yyjson_is_str(v_st));
    ASSERT_STR_EQ(yyjson_get_str(v_st), "ACTIVE");
    yyjson_doc_free(doc);
    free(look_res);

    /* 3. Binding agent self-validation refused */
    const char *bind_bad = "{\"claim_id\":\"b1\",\"theme_id\":\"@org/clean-arch\",\"pinned_version\":\"1.0.0\",\"mode\":\"NORMATIVE\",\"validated_by\":\"agent\"}";
    char *bind_bad_res = handle_binding_claim(srv, bind_bad);
    ASSERT(bind_bad_res != NULL);
    doc = yyjson_read(bind_bad_res, strlen(bind_bad_res), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_bind_code = yyjson_obj_get(payload, "code");
    ASSERT(v_bind_code && yyjson_is_str(v_bind_code));
    ASSERT_STR_EQ(yyjson_get_str(v_bind_code), "BINDING_SELF_VALIDATED");
    yyjson_doc_free(doc);
    free(bind_bad_res);

    /* 4. Binding operator validated admitted */
    const char *bind_good = "{\"claim_id\":\"b1\",\"theme_id\":\"@org/clean-arch\",\"pinned_version\":\"1.0.0\",\"mode\":\"NORMATIVE\",\"validated_by\":\"operator\"}";
    char *bind_good_res = handle_binding_claim(srv, bind_good);
    ASSERT(bind_good_res != NULL);
    doc = yyjson_read(bind_good_res, strlen(bind_good_res), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_adm = yyjson_obj_get(payload, "admitted");
    ASSERT(v_adm && yyjson_get_bool(v_adm) == true);
    yyjson_doc_free(doc);
    free(bind_good_res);

    /* 5. Founding proposal */
    const char *prop_args = "{\"suggested_theme_id\":\"@org/split-payment\",\"namespace\":\"domain\",\"rationale\":\"recurring payment pattern\",\"origin_session\":\"h_sess1\",\"suggested_curator\":\"operator\"}";
    char *prop_res = handle_founding_propose(srv, prop_args);
    ASSERT(prop_res != NULL);
    doc = yyjson_read(prop_res, strlen(prop_res), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_prop = yyjson_obj_get(payload, "proposed");
    ASSERT(v_prop && yyjson_get_bool(v_prop) == true);
    yyjson_val *v_pst = yyjson_obj_get(payload, "status");
    ASSERT(v_pst && yyjson_is_str(v_pst));
    ASSERT_STR_EQ(yyjson_get_str(v_pst), "WAITING_HUMAN");
    yyjson_doc_free(doc);
    free(prop_res);

    /* 6. Founding decide: agent autonomous forbidden */
    const char *dec_bad = "{\"suggested_theme_id\":\"@org/split-payment\",\"operator_accepted\":true,\"is_agent_autonomous\":true}";
    char *dec_bad_res = handle_founding_decide(srv, dec_bad);
    ASSERT(dec_bad_res != NULL);
    doc = yyjson_read(dec_bad_res, strlen(dec_bad_res), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_dec_code = yyjson_obj_get(payload, "code");
    ASSERT(v_dec_code && yyjson_is_str(v_dec_code));
    ASSERT_STR_EQ(yyjson_get_str(v_dec_code), "AUTO_FOUNDING_FORBIDDEN");
    yyjson_doc_free(doc);
    free(dec_bad_res);

    /* 7. Founding decide: operator accepted */
    const char *dec_good = "{\"suggested_theme_id\":\"@org/split-payment\",\"operator_accepted\":true,\"decided_by\":\"operator\",\"is_agent_autonomous\":false}";
    char *dec_good_res = handle_founding_decide(srv, dec_good);
    ASSERT(dec_good_res != NULL);
    doc = yyjson_read(dec_good_res, strlen(dec_good_res), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_dec = yyjson_obj_get(payload, "decided");
    ASSERT(v_dec && yyjson_get_bool(v_dec) == true);
    yyjson_val *v_dst = yyjson_obj_get(payload, "status");
    ASSERT(v_dst && yyjson_is_str(v_dst));
    ASSERT_STR_EQ(yyjson_get_str(v_dst), "DRAFT");
    yyjson_doc_free(doc);
    free(dec_good_res);

    cbm_mcp_server_free(srv);
    PASS();
}

TEST(test_w03_promote_blocked_by_gateway_refusal) {
    cbm_mcp_server_t *srv = cbm_mcp_server_new(NULL);
    ASSERT(srv != NULL);

    /* 1. Open session */
    const char *open_args = "{\"identity\":\"engineer\"}";
    char *open_res = handle_union_session_open(srv, open_args);
    ASSERT(open_res != NULL);
    yyjson_doc *doc = yyjson_read(open_res, strlen(open_res), 0);
    ASSERT(doc != NULL);
    yyjson_val *payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_hid = yyjson_obj_get(payload, "horizon_id");
    ASSERT(v_hid && yyjson_is_str(v_hid));
    char h_id[64];
    strncpy(h_id, yyjson_get_str(v_hid), sizeof(h_id) - 1);
    h_id[sizeof(h_id) - 1] = '\0';
    yyjson_doc_free(doc);
    free(open_res);

    /* 2. Record irreversible action in restricted mode -> blocked, increments refusals */
    char act_args[256];
    snprintf(act_args, sizeof(act_args),
             "{\"horizon_id\":\"%s\",\"action_name\":\"nuke_database\",\"effect_class\":\"IRREVERSIBLE\"}", h_id);
    char *block_res = handle_union_record_action(srv, act_args);
    ASSERT(block_res != NULL);
    doc = yyjson_read(block_res, strlen(block_res), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    yyjson_doc_free(doc);
    free(block_res);

    /* 3. Attempt promotion -> must be blocked with GATEWAY_BLOCKED */
    char prom_args[128];
    snprintf(prom_args, sizeof(prom_args), "{\"horizon_id\":\"%s\"}", h_id);
    char *prom_res = handle_promote_horizon(srv, prom_args, cbm_mcp_server_horizon_pool(srv), NULL);
    ASSERT(prom_res != NULL);
    doc = yyjson_read(prom_res, strlen(prom_res), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_code = yyjson_obj_get(payload, "code");
    ASSERT(v_code && yyjson_is_str(v_code));
    ASSERT_STR_EQ(yyjson_get_str(v_code), "GATEWAY_BLOCKED");
    yyjson_doc_free(doc);
    free(prom_res);

    /* 4. Close session */
    char close_args[128];
    snprintf(close_args, sizeof(close_args), "{\"horizon_id\":\"%s\"}", h_id);
    char *close_res = handle_union_session_close(srv, close_args);
    ASSERT(close_res != NULL);
    free(close_res);

    cbm_mcp_server_free(srv);
    PASS();
}

TEST(test_w07_contest_blind_verify) {
    cbm_mcp_server_t *srv = cbm_mcp_server_new(NULL);
    ASSERT(srv != NULL);

    /* 1. Missing target_ref / evidence -> error */
    char *err_res1 = handle_contest_verify(srv, "{}");
    ASSERT(err_res1 != NULL);
    yyjson_doc *doc = yyjson_read(err_res1, strlen(err_res1), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    yyjson_doc_free(doc);
    free(err_res1);

    char *err_res2 = handle_contest_verify(srv, "{\"target_ref\":\"ref1\",\"evidence\":[]}");
    ASSERT(err_res2 != NULL);
    doc = yyjson_read(err_res2, strlen(err_res2), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    yyjson_doc_free(doc);
    free(err_res2);

    /* 2. INFORMATIVE contest */
    const char *args1 = "{\"target_ref\":\"claim_101\",\"severity\":\"INFORMATIVE\",\"evidence\":[\"cbm://code#func1\"]}";
    char *res1 = handle_contest_verify(srv, args1);
    ASSERT(res1 != NULL);
    doc = yyjson_read(res1, strlen(res1), 0);
    ASSERT(doc != NULL);
    yyjson_val *payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_c = yyjson_obj_get(payload, "contested");
    ASSERT(v_c && yyjson_get_bool(v_c) == true);
    yyjson_val *v_sev = yyjson_obj_get(payload, "severity");
    ASSERT(v_sev && yyjson_is_str(v_sev));
    ASSERT_STR_EQ(yyjson_get_str(v_sev), "informative");
    yyjson_val *v_id1 = yyjson_obj_get(payload, "contest_id");
    ASSERT(v_id1 && yyjson_is_str(v_id1));
    char id1[64];
    strncpy(id1, yyjson_get_str(v_id1), sizeof(id1) - 1);
    id1[sizeof(id1) - 1] = '\0';
    yyjson_doc_free(doc);
    free(res1);

    /* 3. INVALIDATING contest -> monotonic distinct ID */
    const char *args2 = "{\"target_ref\":\"claim_102\",\"severity\":\"INVALIDATING\",\"evidence\":[\"cbm://code#func2\"]}";
    char *res2 = handle_contest_verify(srv, args2);
    ASSERT(res2 != NULL);
    doc = yyjson_read(res2, strlen(res2), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    v_c = yyjson_obj_get(payload, "contested");
    ASSERT(v_c && yyjson_get_bool(v_c) == true);
    v_sev = yyjson_obj_get(payload, "severity");
    ASSERT(v_sev && yyjson_is_str(v_sev));
    ASSERT_STR_EQ(yyjson_get_str(v_sev), "invalidating");
    yyjson_val *v_id2 = yyjson_obj_get(payload, "contest_id");
    ASSERT(v_id2 && yyjson_is_str(v_id2));
    char id2[64];
    strncpy(id2, yyjson_get_str(v_id2), sizeof(id2) - 1);
    id2[sizeof(id2) - 1] = '\0';
    yyjson_doc_free(doc);
    free(res2);

    /* Assert monotonic uniqueness */
    ASSERT(strcmp(id1, id2) != 0);

    cbm_mcp_server_free(srv);
    PASS();
}

TEST(test_w03_promote_blocked_without_session_and_invalidating) {
    cbm_mcp_server_t *srv = cbm_mcp_server_new(NULL);
    ASSERT(srv != NULL);

    const char *h_id = "h_orphan_contest_target";

    /* 1. Register INVALIDATING contest against horizon without open session */
    char contest_args[256];
    snprintf(contest_args, sizeof(contest_args),
             "{\"target_ref\":\"%s\",\"severity\":\"INVALIDATING\",\"evidence\":[\"critical_security_flaw\"]}", h_id);
    char *contest_res = handle_contest_verify(srv, contest_args);
    ASSERT(contest_res != NULL);
    yyjson_doc *doc = yyjson_read(contest_res, strlen(contest_res), 0);
    ASSERT(doc != NULL);
    yyjson_val *payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_c = yyjson_obj_get(payload, "contested");
    ASSERT(v_c && yyjson_get_bool(v_c) == true);
    yyjson_doc_free(doc);
    free(contest_res);

    /* 2. Promote horizon directly without session -> MUST be blocked by contest */
    char prom_args[256];
    snprintf(prom_args, sizeof(prom_args), "{\"horizon_id\":\"%s\"}", h_id);
    char *prom_res = handle_promote_horizon(srv, prom_args, cbm_mcp_server_horizon_pool(srv), NULL);
    ASSERT(prom_res != NULL);
    doc = yyjson_read(prom_res, strlen(prom_res), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_code = yyjson_obj_get(payload, "code");
    ASSERT(v_code && yyjson_is_str(v_code));
    ASSERT_STR_EQ(yyjson_get_str(v_code), "CONTEST_BLOCKED");
    yyjson_doc_free(doc);
    free(prom_res);

    cbm_mcp_server_free(srv);
    PASS();
}

TEST(test_w05_founding_decline_and_metadata_preservation) {
    cbm_mcp_server_t *srv = cbm_mcp_server_new(NULL);
    ASSERT(srv != NULL);

    /* 1. Propose theme with custom namespace and rationale */
    const char *prop_args = "{\"suggested_theme_id\":\"@org/custom-pattern\",\"namespace\":\"architecture\",\"rationale\":\"actor pattern\",\"origin_session\":\"h_sess_99\",\"suggested_curator\":\"alice\"}";
    char *prop_res = handle_founding_propose(srv, prop_args);
    ASSERT(prop_res != NULL);
    yyjson_doc *doc = yyjson_read(prop_res, strlen(prop_res), 0);
    ASSERT(doc != NULL);
    yyjson_val *payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_prop = yyjson_obj_get(payload, "proposed");
    ASSERT(v_prop && yyjson_get_bool(v_prop) == true);
    yyjson_doc_free(doc);
    free(prop_res);

    /* 2. Decline proposal -> must return status DECLINED and accepted false */
    const char *dec_bad = "{\"suggested_theme_id\":\"@org/custom-pattern\",\"operator_accepted\":false,\"decided_by\":\"operator\",\"is_agent_autonomous\":false}";
    char *dec_bad_res = handle_founding_decide(srv, dec_bad);
    ASSERT(dec_bad_res != NULL);
    doc = yyjson_read(dec_bad_res, strlen(dec_bad_res), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_dec = yyjson_obj_get(payload, "decided");
    ASSERT(v_dec && yyjson_get_bool(v_dec) == true);
    yyjson_val *v_acc = yyjson_obj_get(payload, "accepted");
    ASSERT(v_acc && yyjson_get_bool(v_acc) == false);
    yyjson_val *v_st = yyjson_obj_get(payload, "status");
    ASSERT(v_st && yyjson_is_str(v_st));
    ASSERT_STR_EQ(yyjson_get_str(v_st), "DECLINED");
    yyjson_doc_free(doc);
    free(dec_bad_res);

    /* 3. Re-propose and accept -> must preserve namespace 'architecture' */
    char *prop_res2 = handle_founding_propose(srv, prop_args);
    free(prop_res2);

    const char *dec_good = "{\"suggested_theme_id\":\"@org/custom-pattern\",\"operator_accepted\":true,\"decided_by\":\"operator\",\"is_agent_autonomous\":false}";
    char *dec_good_res = handle_founding_decide(srv, dec_good);
    ASSERT(dec_good_res != NULL);
    doc = yyjson_read(dec_good_res, strlen(dec_good_res), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_acc2 = yyjson_obj_get(payload, "accepted");
    ASSERT(v_acc2 && yyjson_get_bool(v_acc2) == true);
    yyjson_val *v_st2 = yyjson_obj_get(payload, "status");
    ASSERT(v_st2 && yyjson_is_str(v_st2));
    ASSERT_STR_EQ(yyjson_get_str(v_st2), "DRAFT");
    yyjson_doc_free(doc);
    free(dec_good_res);

    /* 4. Lookup theme -> namespace must be preserved as 'architecture' */
    char *look_res = handle_theme_lookup(srv, "{\"theme_id\":\"@org/custom-pattern\"}");
    ASSERT(look_res != NULL);
    doc = yyjson_read(look_res, strlen(look_res), 0);
    ASSERT(doc != NULL);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_ns = yyjson_obj_get(payload, "namespace");
    ASSERT(v_ns && yyjson_is_str(v_ns));
    ASSERT_STR_EQ(yyjson_get_str(v_ns), "architecture");
    yyjson_doc_free(doc);
    free(look_res);

    cbm_mcp_server_free(srv);
    PASS();
}

TEST(test_w01_session_close_sweep_and_trace) {
    cbm_mcp_server_t *srv = cbm_mcp_server_new(NULL);
    ASSERT(srv != NULL);

    /* 1. Open session */
    const char *open_args = "{\"identity\":\"architect_lead\"}";
    char *open_res = handle_union_session_open(srv, open_args);
    ASSERT(open_res != NULL);
    yyjson_doc *doc = yyjson_read(open_res, strlen(open_res), 0);
    ASSERT(doc != NULL);
    yyjson_val *payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_hid = yyjson_obj_get(payload, "horizon_id");
    ASSERT(v_hid && yyjson_is_str(v_hid));
    char h_id[64];
    strncpy(h_id, yyjson_get_str(v_hid), sizeof(h_id) - 1);
    h_id[sizeof(h_id) - 1] = '\0';
    yyjson_doc_free(doc);
    free(open_res);

    /* 2. Record actions across effect classes: 1 IDEMPOTENT, 1 COMPENSABLE */
    char act1[256];
    snprintf(act1, sizeof(act1),
             "{\"horizon_id\":\"%s\",\"action_name\":\"read_config\",\"effect_class\":\"IDEMPOTENT\"}", h_id);
    char *res_act1 = handle_union_record_action(srv, act1);
    ASSERT(res_act1 != NULL);
    free(res_act1);

    char act2[256];
    snprintf(act2, sizeof(act2),
             "{\"horizon_id\":\"%s\",\"action_name\":\"stage_patch\",\"effect_class\":\"COMPENSABLE\"}", h_id);
    char *res_act2 = handle_union_record_action(srv, act2);
    ASSERT(res_act2 != NULL);
    free(res_act2);

    /* 3. Capture claim via pure MCP tool union_claim_capture */
    char cap_args[512];
    snprintf(cap_args, sizeof(cap_args),
             "{\"horizon_id\":\"%s\",\"claim_id\":\"claim_unresolved_42\",\"type\":\"DECISION\",\"predicate\":\"adopt event sourcing for order tracking.\",\"consequence\":\"Data divergence if not audited\",\"based_on_seq\":\"seq_1\"}", h_id);
    char *cap_res = handle_union_claim_capture(srv, cap_args);
    ASSERT(cap_res != NULL);
    doc = yyjson_read(cap_res, strlen(cap_res), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == false);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_cap = yyjson_obj_get(payload, "captured");
    ASSERT(v_cap && yyjson_get_bool(v_cap) == true);
    yyjson_doc_free(doc);
    free(cap_res);

    /* 4. Attempt to close session while claim is unresolved -> MUST fail with SWEEP_INCOMPLETE */
    char close_args[128];
    snprintf(close_args, sizeof(close_args), "{\"horizon_id\":\"%s\",\"reason\":\"NORMAL\"}", h_id);
    char *close_res = handle_union_session_close(srv, close_args);
    ASSERT(close_res != NULL);
    doc = yyjson_read(close_res, strlen(close_res), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_code = yyjson_obj_get(payload, "code");
    ASSERT(v_code && yyjson_is_str(v_code));
    ASSERT_STR_EQ(yyjson_get_str(v_code), "SWEEP_INCOMPLETE");
    yyjson_val *v_msg = yyjson_obj_get(payload, "message");
    ASSERT(v_msg && yyjson_is_str(v_msg));
    ASSERT(strstr(yyjson_get_str(v_msg), "claim_unresolved_42") != NULL);
    yyjson_doc_free(doc);
    free(close_res);

    /* 5. Resolve claim via pure MCP tool union_claim_resolve with DISCARDED */
    char res_args[256];
    snprintf(res_args, sizeof(res_args),
             "{\"horizon_id\":\"%s\",\"claim_id\":\"claim_unresolved_42\",\"destination\":\"DISCARDED\",\"owner_or_reason\":\"exploration\"}", h_id);
    char *res_res = handle_union_claim_resolve(srv, res_args);
    ASSERT(res_res != NULL);
    doc = yyjson_read(res_res, strlen(res_res), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == false);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_res = yyjson_obj_get(payload, "resolved");
    ASSERT(v_res && yyjson_get_bool(v_res) == true);
    yyjson_doc_free(doc);
    free(res_res);

    /* 6. Close session now -> MUST succeed, log trace to host, and include factual trace object */
    close_res = handle_union_session_close(srv, close_args);
    ASSERT(close_res != NULL);
    doc = yyjson_read(close_res, strlen(close_res), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == false);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_ok = yyjson_obj_get(payload, "success");
    ASSERT(v_ok && yyjson_get_bool(v_ok) == true);

    yyjson_val *v_trace = yyjson_obj_get(payload, "trace");
    ASSERT(v_trace != NULL && yyjson_is_obj(v_trace));
    yyjson_val *v_thid = yyjson_obj_get(v_trace, "horizon_id");
    ASSERT(v_thid && yyjson_is_str(v_thid));
    ASSERT_STR_EQ(yyjson_get_str(v_thid), h_id);
    yyjson_val *v_tout = yyjson_obj_get(v_trace, "outcome");
    ASSERT(v_tout && yyjson_is_str(v_tout));
    ASSERT_STR_EQ(yyjson_get_str(v_tout), "NORMAL");

    yyjson_val *v_acts = yyjson_obj_get(v_trace, "actions");
    ASSERT(v_acts != NULL && yyjson_is_obj(v_acts));
    yyjson_val *v_tot = yyjson_obj_get(v_acts, "total");
    ASSERT(v_tot && yyjson_get_int(v_tot) == 2);
    yyjson_val *v_idem = yyjson_obj_get(v_acts, "idempotent");
    ASSERT(v_idem && yyjson_get_int(v_idem) == 1);
    yyjson_val *v_comp = yyjson_obj_get(v_acts, "compensable");
    ASSERT(v_comp && yyjson_get_int(v_comp) == 1);
    yyjson_val *v_irrev = yyjson_obj_get(v_acts, "irreversible");
    ASSERT(v_irrev && yyjson_get_int(v_irrev) == 0);

    yyjson_val *v_excl = yyjson_obj_get(v_trace, "exclusions");
    ASSERT(v_excl && yyjson_is_obj(v_excl));
    yyjson_val *v_decl = yyjson_obj_get(v_excl, "declared");
    ASSERT(v_decl && yyjson_get_bool(v_decl) == true);
    yyjson_val *v_oos = yyjson_obj_get(v_excl, "out_of_scope");
    ASSERT(v_oos && yyjson_get_int(v_oos) == 1);
    yyjson_doc_free(doc);
    free(close_res);

    cbm_mcp_server_free(srv);
    PASS();
}

TEST(test_w05_founding_requires_proposal_and_operator_identity) {
    cbm_mcp_server_t *srv = cbm_mcp_server_new(NULL);
    ASSERT(srv != NULL);
    yyjson_val *payload = NULL;

    /* 1. Decide without any prior proposal -> MUST return PROPOSAL_NOT_FOUND */
    const char *dec_unproposed = "{\"suggested_theme_id\":\"@org/unproposed-theme\",\"operator_accepted\":true,\"decided_by\":\"operator\"}";
    char *res_unprop = handle_founding_decide(srv, dec_unproposed);
    ASSERT(res_unprop != NULL);
    yyjson_doc *doc = yyjson_read(res_unprop, strlen(res_unprop), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_code = yyjson_obj_get(payload, "code");
    ASSERT(v_code && yyjson_is_str(v_code));
    ASSERT_STR_EQ(yyjson_get_str(v_code), "PROPOSAL_NOT_FOUND");
    yyjson_doc_free(doc);
    free(res_unprop);

    /* 2. Submit proposal */
    const char *prop_args = "{\"suggested_theme_id\":\"@org/strict-authority\",\"namespace\":\"core\",\"rationale\":\"strict operator check\",\"origin_session\":\"h_sess_auth\",\"suggested_curator\":\"lead\"}";
    char *res_prop = handle_founding_propose(srv, prop_args);
    ASSERT(res_prop != NULL);
    free(res_prop);

    /* 3. Decide with non-operator identity (e.g. agent) -> MUST fail with AUTO_FOUNDING_FORBIDDEN */
    const char *dec_agent = "{\"suggested_theme_id\":\"@org/strict-authority\",\"operator_accepted\":true,\"decided_by\":\"agent_autonomous\"}";
    char *res_agent = handle_founding_decide(srv, dec_agent);
    ASSERT(res_agent != NULL);
    doc = yyjson_read(res_agent, strlen(res_agent), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    v_code = yyjson_obj_get(payload, "code");
    ASSERT(v_code && yyjson_is_str(v_code));
    ASSERT_STR_EQ(yyjson_get_str(v_code), "AUTO_FOUNDING_FORBIDDEN");
    yyjson_doc_free(doc);
    free(res_agent);

    /* 4. Decide with operator identity -> succeeds */
    const char *dec_ok = "{\"suggested_theme_id\":\"@org/strict-authority\",\"operator_accepted\":true,\"decided_by\":\"operator_chief\"}";
    char *res_ok = handle_founding_decide(srv, dec_ok);
    ASSERT(res_ok != NULL);
    doc = yyjson_read(res_ok, strlen(res_ok), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == false);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    yyjson_val *v_dec = yyjson_obj_get(payload, "decided");
    ASSERT(v_dec && yyjson_get_bool(v_dec) == true);
    yyjson_doc_free(doc);
    free(res_ok);

    /* 5. Repeat decide on consumed proposal -> MUST return PROPOSAL_NOT_FOUND */
    char *res_repeat = handle_founding_decide(srv, dec_ok);
    ASSERT(res_repeat != NULL);
    doc = yyjson_read(res_repeat, strlen(res_repeat), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == true);
    payload = get_payload(doc);
    ASSERT(payload != NULL);
    v_code = yyjson_obj_get(payload, "code");
    ASSERT(v_code && yyjson_is_str(v_code));
    ASSERT_STR_EQ(yyjson_get_str(v_code), "PROPOSAL_NOT_FOUND");
    yyjson_doc_free(doc);
    free(res_repeat);

    cbm_mcp_server_free(srv);
    PASS();
}

TEST(test_w07_contest_prefix_delimiter_isolation) {
    cbm_mcp_server_t *srv = cbm_mcp_server_new(NULL);
    ASSERT(srv != NULL);

    /* 1. Submit BLOCKING contest against target "horizon_123_410" */
    const char *contest_410 = "{\"target_ref\":\"horizon_123_410\",\"severity\":\"BLOCKING\",\"evidence\":[\"cbm://logs#audit_violation\"]}";
    char *res_c = handle_contest_verify(srv, contest_410);
    ASSERT(res_c != NULL);
    yyjson_doc *doc = yyjson_read(res_c, strlen(res_c), 0);
    ASSERT(doc != NULL);
    ASSERT(get_is_error(doc) == false);
    yyjson_doc_free(doc);
    free(res_c);

    /* 2. Check that target "horizon_123_41" is NOT blocked (prefix match without delimiter must not match!) */
    size_t blocked_41 = cbm_contest_is_blocked(cbm_mcp_server_contest_registry(srv), "horizon_123_41");
    ASSERT_EQ(blocked_41, 0);

    /* 3. Check that "horizon_123_410" IS blocked */
    size_t blocked_410 = cbm_contest_is_blocked(cbm_mcp_server_contest_registry(srv), "horizon_123_410");
    ASSERT_EQ(blocked_410, 1);

    /* 4. Submit BLOCKING contest against subpath with delimiter: "horizon_123_41/subpart" */
    const char *contest_sub = "{\"target_ref\":\"horizon_123_41/subpart\",\"severity\":\"BLOCKING\",\"evidence\":[\"cbm://logs#subpart_flaw\"]}";
    char *res_sub = handle_contest_verify(srv, contest_sub);
    ASSERT(res_sub != NULL);
    free(res_sub);

    /* Now horizon_123_41 IS blocked because of delimiter '/' match */
    size_t blocked_after_sub = cbm_contest_is_blocked(cbm_mcp_server_contest_registry(srv), "horizon_123_41");
    ASSERT_EQ(blocked_after_sub, 1);

    /* 5. Submit BLOCKING contest against a claim associated with an explicit target_horizon */
    const char *contest_explicit = "{\"target_ref\":\"claim_iso_99\",\"target_horizon\":\"horizon_iso_explicit\",\"severity\":\"BLOCKING\",\"evidence\":[\"cbm://evidence#rule\"]}";
    char *res_exp = handle_contest_verify(srv, contest_explicit);
    ASSERT(res_exp != NULL);
    free(res_exp);

    /* The explicit horizon MUST be blocked */
    size_t blocked_explicit = cbm_contest_is_blocked(cbm_mcp_server_contest_registry(srv), "horizon_iso_explicit");
    ASSERT_EQ(blocked_explicit, 1);

    cbm_mcp_server_free(srv);
    PASS();
}

SUITE(union_workflow_e2e) {
    RUN_TEST(test_w01_session_lifecycle);
    RUN_TEST(test_w01_session_close_sweep_and_trace);
    RUN_TEST(test_w02_action_gateway_restricted_mode);
    RUN_TEST(test_w03_promote_blocked_by_active_contest);
    RUN_TEST(test_w03_promote_blocked_by_gateway_refusal);
    RUN_TEST(test_w03_promote_blocked_without_session_and_invalidating);
    RUN_TEST(test_w04_routing_and_provenance_validation);
    RUN_TEST(test_w05_territory_and_founding);
    RUN_TEST(test_w05_founding_decline_and_metadata_preservation);
    RUN_TEST(test_w05_founding_requires_proposal_and_operator_identity);
    RUN_TEST(test_w07_contest_blind_verify);
    RUN_TEST(test_w07_contest_prefix_delimiter_isolation);
}

