#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <signal.h>
#include <ps5/kernel.h>
#include <ps5/mdbg.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "auth_dat.h"
#include "config_dat.h"

#ifndef AGENT_PORT
#define AGENT_PORT 31337
#endif

#define ACCOUNT_NUMB_MAX 16
#define ACCOUNT_TYPE_MAX 17
#define ACCOUNT_NAME_MAX 32
#define SCE_REGMGR_ENT_KEY_REMOTEPLAY_rp_enable 1098973184

typedef struct notify_request {
    char useless1[45];
    char message[3075];
} notify_request_t;

int sceKernelSendNotificationRequest(int, notify_request_t*, size_t, int);

int sceUserServiceInitialize(void *);
int sceUserServiceGetForegroundUser(int *);
int sceSystemServiceParamGetInt(int, int *);

int sceRegMgrGetInt(int, int*);
int sceRegMgrGetStr(int, char*, size_t);
int sceRegMgrGetBin(int, void*, size_t);
int sceRegMgrSetBin(int, const void*, size_t);
int sceRegMgrSetInt(int, int);
int sceRegMgrSetStr(int, const char*, size_t);

static void notifyf(const char *msg)
{
    notify_request_t req;
    memset(&req, 0, sizeof(req));
    snprintf(req.message, sizeof(req.message), "%s", msg);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

static int ent_num(int a, int b, int c, int d, int e)
{
    if (a < 1 || a > b) {
        return e;
    }

    return (a - 1) * c + d;
}

static int key_user_id(int slot)
{
    return ent_num(slot, 16U, 65536U, 125829376U, 127140096U);
}

static int key_account_name(int slot)
{
    return ent_num(slot, 16U, 65536U, 125829632U, 127140352U);
}

static int key_account_id(int slot)
{
    return ent_num(slot, 16U, 65536U, 125830400U, 127141120U);
}

static int key_account_flags(int slot)
{
    return ent_num(slot, 16U, 65536U, 125831168U, 127141888U);
}

static int key_account_type(int slot)
{
    return ent_num(slot, 16U, 65536U, 125874183U, 127184903U);
}

static int key_user_rp_enable(int slot)
{
    return ent_num(slot, 16U, 65536U, 125859841U, 127170561U);
}

static int key_account_signin_id(int slot)
{
    return ent_num(slot, 16U, 65536U, 125830656U, 127141376U);
}

static int get_account_name(int slot, char name[ACCOUNT_NAME_MAX])
{
    memset(name, 0, ACCOUNT_NAME_MAX);
    return sceRegMgrGetStr(key_account_name(slot), name, ACCOUNT_NAME_MAX);
}

static int get_account_type(int slot, char type[ACCOUNT_TYPE_MAX])
{
    memset(type, 0, ACCOUNT_TYPE_MAX);
    return sceRegMgrGetStr(key_account_type(slot), type, ACCOUNT_TYPE_MAX);
}

static int get_account_id(int slot, uint64_t *id)
{
    *id = 0;
    return sceRegMgrGetBin(key_account_id(slot), id, sizeof(uint64_t));
}

static int set_account_id(int slot, uint64_t id)
{
    return sceRegMgrSetBin(key_account_id(slot), &id, sizeof(uint64_t));
}

static int set_account_type(int slot, const char *type)
{
    return sceRegMgrSetStr(key_account_type(slot), type, ACCOUNT_TYPE_MAX);
}

static int set_account_flags(int slot, int flags)
{
    return sceRegMgrSetInt(key_account_flags(slot), flags);
}

static int set_user_rp_enable(int slot, int val)
{
    return sceRegMgrSetInt(key_user_rp_enable(slot), val);
}

static int get_account_flags(int slot, int *flags)
{
    *flags = 0;
    return sceRegMgrGetInt(key_account_flags(slot), flags);
}

static int get_user_id(int slot, int *user_id)
{
    *user_id = 0;
    return sceRegMgrGetInt(key_user_id(slot), user_id);
}

static int get_current_slot(void)
{
    int fg_user = 0;

    sceUserServiceInitialize(0);

    if (sceUserServiceGetForegroundUser(&fg_user) != 0) {
        return -1;
    }

    for (int slot = 1; slot <= ACCOUNT_NUMB_MAX; slot++) {
        int user_id = 0;

        if (get_user_id(slot, &user_id) == 0 && user_id == fg_user) {
            return slot;
        }
    }

    return -1;
}

static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void base64_encode_8bytes(const uint8_t in[8], char out[13])
{
    int i = 0;
    int j = 0;
    int len = 8;
    int out_len = 4 * ((len + 2) / 3);

    while (i < len) {
        uint32_t a = i < len ? in[i++] : 0;
        uint32_t b = i < len ? in[i++] : 0;
        uint32_t c = i < len ? in[i++] : 0;

        uint32_t triple = (a << 16) | (b << 8) | c;

        out[j++] = b64_table[(triple >> 18) & 0x3F];
        out[j++] = b64_table[(triple >> 12) & 0x3F];
        out[j++] = b64_table[(triple >> 6) & 0x3F];
        out[j++] = b64_table[triple & 0x3F];
    }

    for (int k = 0; k < (3 - len % 3) % 3; k++) {
        out[out_len - 1 - k] = '=';
    }

    out[out_len] = '\0';
}

static void account_id_to_base64(uint64_t id, char out[13])
{
    uint8_t raw[8];

    raw[0] = (uint8_t)((id >> 0) & 0xff);
    raw[1] = (uint8_t)((id >> 8) & 0xff);
    raw[2] = (uint8_t)((id >> 16) & 0xff);
    raw[3] = (uint8_t)((id >> 24) & 0xff);
    raw[4] = (uint8_t)((id >> 32) & 0xff);
    raw[5] = (uint8_t)((id >> 40) & 0xff);
    raw[6] = (uint8_t)((id >> 48) & 0xff);
    raw[7] = (uint8_t)((id >> 56) & 0xff);

    base64_encode_8bytes(raw, out);
}

static int send_all(int fd, const char *s)
{
    size_t len = strlen(s);

    while (len > 0) {
        ssize_t n = send(fd, s, len, 0);

        if (n <= 0) {
            return -1;
        }

        s += n;
        len -= n;
    }

    return 0;
}

static void send_account_line(int fd, int slot)
{
    char name[ACCOUNT_NAME_MAX];
    char type[ACCOUNT_TYPE_MAX];
    uint64_t id = 0;
    int flags = 0;
    int user_id = 0;
    int current = 0;
    char b64[13];
    char line[1024];

    memset(name, 0, sizeof(name));
    memset(type, 0, sizeof(type));
    memset(b64, 0, sizeof(b64));

    if (get_account_name(slot, name) != 0 || name[0] == '\0') {
        return;
    }

    get_account_type(slot, type);
    get_account_id(slot, &id);
    get_account_flags(slot, &flags);
    get_user_id(slot, &user_id);

    char signin_id[65];
    memset(signin_id, 0, sizeof(signin_id));
    sceRegMgrGetStr(key_account_signin_id(slot), signin_id, sizeof(signin_id));

    int user_status = 0;
    sceRegMgrGetInt(ent_num(slot, 16U, 65536U, 125832960U, 127143680U), &user_status);

    int created_version = 0;
    sceRegMgrGetInt(ent_num(slot, 16U, 65536U, 125832704U, 127143424U), &created_version);

    char online_id[17];
    memset(online_id, 0, sizeof(online_id));
    sceRegMgrGetStr(ent_num(slot, 16U, 65536U, 125874188U, 127184908U), online_id, sizeof(online_id));

    char np_id[65];
    memset(np_id, 0, sizeof(np_id));
    sceRegMgrGetStr(ent_num(slot, 16U, 65536U, 125874189U, 127184909U), np_id, sizeof(np_id));

    current = slot == get_current_slot() ? 1 : 0;
    account_id_to_base64(id, b64);

    snprintf(
        line,
        sizeof(line),
        "USER slot=%d current=%d user_id=%d name=\"%s\" type=\"%s\" flags=0x%04x id=0x%016" PRIx64 " b64=%s signin=\"%s\" status=%d ver=0x%08x online_id=\"%s\" np_id=\"%s\"\n",
        slot,
        current,
        user_id,
        name,
        type[0] ? type : "",
        (unsigned int)flags,
        id,
        b64,
        signin_id,
        user_status,
        created_version,
        online_id,
        np_id
    );

    send_all(fd, line);
}

static void cmd_list(int fd)
{
    send_all(fd, "OK LIST\n");

    for (int slot = 1; slot <= ACCOUNT_NUMB_MAX; slot++) {
        send_account_line(fd, slot);
    }

    send_all(fd, "END\n");
}

static void cmd_current(int fd)
{
    int slot = get_current_slot();

    if (slot < 1 || slot > ACCOUNT_NUMB_MAX) {
        send_all(fd, "ERR failed_to_get_current_user\nEND\n");
        return;
    }

    send_all(fd, "OK CURRENT\n");
    send_account_line(fd, slot);
    send_all(fd, "END\n");
}

static void cmd_b64(int fd, const char *hex)
{
    uint64_t id = 0;
    char b64[13];
    char line[256];

    if (!hex || !hex[0]) {
        send_all(fd, "ERR missing_hex\nEND\n");
        return;
    }

    id = strtoull(hex, NULL, 0);
    account_id_to_base64(id, b64);

    snprintf(
        line,
        sizeof(line),
        "OK B64 hex=0x%016" PRIx64 " b64=%s\nEND\n",
        id,
        b64
    );

    send_all(fd, line);
}

static void cmd_setid(int fd, const char *slot_s, const char *hex)
{
    int slot = 0;
    uint64_t old_id = 0;
    uint64_t new_id = 0;
    uint64_t verify_id = 0;

    char name[ACCOUNT_NAME_MAX];
    char old_b64[13];
    char new_b64[13];
    char verify_b64[13];
    char line[768];

    int r_old = 0;
    int r_set = 0;
    int r_verify = 0;

    if (!slot_s || !hex) {
        send_all(fd, "ERR usage_SETID_slot_hex\nEND\n");
        return;
    }

    slot = atoi(slot_s);

    if (slot < 1 || slot > ACCOUNT_NUMB_MAX) {
        send_all(fd, "ERR invalid_slot\nEND\n");
        return;
    }

    memset(name, 0, sizeof(name));

    if (get_account_name(slot, name) != 0 || name[0] == '\0') {
        send_all(fd, "ERR empty_or_invalid_account\nEND\n");
        return;
    }

    new_id = strtoull(hex, NULL, 0);

    r_old = get_account_id(slot, &old_id);
    r_set = set_account_id(slot, new_id);
    int r_set_type = set_account_type(slot, "np");
    int r_set_flags = set_account_flags(slot, 4098);
    int r_set_rp = set_user_rp_enable(slot, 1);
    int r_set_global_rp = sceRegMgrSetInt(SCE_REGMGR_ENT_KEY_REMOTEPLAY_rp_enable, 1);
    r_verify = get_account_id(slot, &verify_id);

    int verify_flags_val = 0;
    char verify_type_val[ACCOUNT_TYPE_MAX];
    memset(verify_type_val, 0, sizeof(verify_type_val));

    get_account_flags(slot, &verify_flags_val);
    get_account_type(slot, verify_type_val);

    account_id_to_base64(old_id, old_b64);
    account_id_to_base64(new_id, new_b64);
    account_id_to_base64(verify_id, verify_b64);

    snprintf(
        line,
        sizeof(line),
        "OK SETID\n"
        "slot=%d\n"
        "name=\"%s\"\n"
        "old_id=0x%016" PRIx64 "\n"
        "old_b64=%s\n"
        "new_id=0x%016" PRIx64 "\n"
        "new_b64=%s\n"
        "verify_id=0x%016" PRIx64 "\n"
        "verify_b64=%s\n"
        "verify_type=\"%s\"\n"
        "verify_flags=0x%04x\n"
        "ret_old_set_verify=%d/%d/%d\n"
        "ret_type_flags_rp=%d/%d/%d/%d\n"
        "reboot_recommended=1\n"
        "END\n",
        slot,
        name,
        old_id,
        old_b64,
        new_id,
        new_b64,
        verify_id,
        verify_b64,
        verify_type_val,
        (unsigned int)verify_flags_val,
        r_old,
        r_set,
        r_verify,
        r_set_type,
        r_set_flags,
        r_set_rp,
        r_set_global_rp
    );

    send_all(fd, line);

    notifyf("ActRemoteLink: account_id e Remote Play global ativados. Reinicie o PS5.");
}


static void cmd_pin(int fd)
{
    send_all(
        fd,
        "ERR PIN integrated_agent_disabled_use_actremotelink_pin_elf\n"
        "END\n"
    );
}


static void cmd_prepare_pin(int fd)
{
    int current_slot = -1;
    uint64_t account_id = 0;
    char account_b64[13];
    int32_t rp_enable_val = 0;
    int32_t verify_val = 0;
    char line[1024];

    memset(account_b64, 0, sizeof(account_b64));
    memset(line, 0, sizeof(line));

    current_slot = get_current_slot();
    if (current_slot < 1 || current_slot > ACCOUNT_NUMB_MAX) {
        send_all(fd, "ERR PREPARE_PIN failed_get_current_slot\nEND\n");
        return;
    }

    if (get_account_id(current_slot, &account_id) != 0) {
        send_all(fd, "ERR PREPARE_PIN failed_get_account_id\nEND\n");
        return;
    }

    account_id_to_base64(account_id, account_b64);

    if (sceRegMgrGetInt(SCE_REGMGR_ENT_KEY_REMOTEPLAY_rp_enable, &rp_enable_val)) {
        send_all(fd, "ERR PREPARE_PIN failed_get_remoteplay_enable\nEND\n");
        return;
    }

    if (rp_enable_val != 1) {
        if (sceRegMgrSetInt(SCE_REGMGR_ENT_KEY_REMOTEPLAY_rp_enable, 1)) {
            send_all(fd, "ERR PREPARE_PIN failed_set_remoteplay_enable\nEND\n");
            return;
        }
    }

    int32_t user_rp_val = 0;
    if (sceRegMgrGetInt(key_user_rp_enable(current_slot), &user_rp_val) == 0 && user_rp_val != 1) {
        sceRegMgrSetInt(key_user_rp_enable(current_slot), 1);
    }

    if (sceRegMgrGetInt(SCE_REGMGR_ENT_KEY_REMOTEPLAY_rp_enable, &verify_val)) {
        send_all(fd, "ERR PREPARE_PIN failed_verify_remoteplay_enable\nEND\n");
        return;
    }

    snprintf(
        line,
        sizeof(line),
        "OK PREPARE_PIN\n"
        "slot=%d\n"
        "remoteplay_enable=%d\n"
        "account_id_hex=0x%016" PRIx64 "\n"
        "account_id_base64=%s\n"
        "next_step=Open Settings > System > Remote Play > Link Device\n"
        "END\n",
        current_slot,
        verify_val,
        account_id,
        account_b64
    );

    send_all(fd, line);

    snprintf(
        line,
        sizeof(line),
        "ActRemoteLink pronto\\nID: %s\\nAbra Remote Play > Vincular dispositivo",
        account_b64
    );

    notifyf(line);
}


struct reg_key_desc {
    const char *name;
    int offset;
    int type; // 0=int, 1=str, 2=bin
    int size;
} compare_keys[] = {
    {"user_id", 125829376, 0, 4},
    {"user_name", 125829632, 1, 17},
    {"passcode", 125829888, 1, 16},
    {"auto_login", 125830144, 0, 4},
    {"account_id", 125830400, 2, 8},
    {"signin_id", 125830656, 1, 65},
    {"notification", 125830912, 0, 4},
    {"login_flag", 125831168, 0, 4},
    {"last_login_orde", 125831424, 0, 4},
    {"discplayer_flag", 125831680, 0, 4},
    {"friend_flag", 125831936, 0, 4},
    {"app_sort_order", 125832448, 0, 4},
    {"created_version", 125832704, 0, 4},
    {"user_status", 125832960, 0, 4},
    {"notifi_behavior", 125833216, 0, 4},
    {"friend_cllf", 125833472, 2, 8},
    {"platformprivacy_ws1", 125833728, 0, 4},
    {"notification_settings", 125833984, 0, 4},
    {"notification_settings_1", 125834240, 0, 4},
    {"notification_settings_2", 125834496, 0, 4},
    {"notification_settings_3", 125834752, 0, 4},
    {"ftux_busy_hint", 125835008, 0, 4},
    {"chat_status", 125835264, 0, 4},
    {"hint_show_flag", 125835520, 0, 4},
    {"account_remarks", 125845504, 1, 32},
    {"PARENTAL_game", 125852673, 0, 4},
    {"PARENTAL_bd_age", 125852674, 0, 4},
    {"PARENTAL_dvd_region", 125852675, 0, 4},
    {"PARENTAL_dvd", 125852676, 0, 4},
    {"PARENTAL_browser", 125852677, 0, 4},
    {"PARENTAL_morpheus", 125852678, 0, 4},
    {"PARENTAL_game_age_limit", 125852679, 0, 4},
    {"PARENTAL_content_control", 125852680, 0, 4},
    {"PARENTAL_game_age_limit_region", 125852681, 1, 3},
    {"PARENTAL_game_white_list", 125852736, 1, 2000},
    {"TOPMENU_limit_item", 125854721, 0, 4},
    {"TOPMENU_tutorial_flag", 125854722, 0, 4},
    {"TOPMENU_notificatn_flag", 125854723, 0, 4},
    {"CAPTUREMENU_status", 125854977, 0, 4},
    {"CONTROL_CENTER_function_control_settings", 125855233, 2, 64},
    {"ACCESSIBILITY_zoom", 125856769, 0, 4},
    {"ACCESSIBILITY_keyremap_enable", 125856770, 0, 4},
    {"ACCESSIBILITY_keyremap_data", 125856771, 2, 16},
    {"ACCESSIBILITY_long_press_time", 125856772, 0, 4},
    {"ACCESSIBILITY_zoom_set_focus", 125856773, 0, 4},
    {"REMOTEPLAY_rp_enable", 125859841, 0, 4},
    {"SHAREPLAY_framerate_host", 125860865, 0, 4},
    {"SHAREPLAY_resolution_host", 125860866, 0, 4},
    {"SHAREPLAY_flags", 125860867, 0, 4},
    {"NP_fake_plus", 125874177, 0, 4},
    {"NP_is_quick_signup", 125874178, 0, 4},
    {"NP_age_verified", 125874179, 0, 4},
    {"NP_auth_error_flag", 125874180, 0, 4},
    {"NP_psnpw_for_debug", 125874181, 1, 31},
    {"NP_env", 125874183, 1, 17},
    {"NP_offl_account_id", 125874184, 1, 37},
    {"NP_sub_account", 125874185, 0, 4},
    {"NP_offl_acct_adult", 125874186, 0, 4},
    {"NP_acct_upg_flag", 125874187, 0, 4},
    {"NP_online_id", 125874188, 1, 17},
    {"NP_np_id", 125874189, 1, 65},
    {"NP_country_code", 125874190, 1, 3},
    {"NP_language_code", 125874191, 1, 6},
    {"NP_language_code_2", 125874192, 1, 36},
    {"NP_date_of_birth", 125874193, 1, 11},
    {"NP_age", 125874194, 0, 4},
    {"NP_m_account_id", 125874195, 1, 65},
    {"NP_fake_premium", 125874240, 0, 4},
};

static void cmd_compare(int fd)
{
    send_all(fd, "OK COMPARE\n");

    int num_keys = sizeof(compare_keys) / sizeof(compare_keys[0]);

    for (int i = 0; i < num_keys; i++) {
        struct reg_key_desc key = compare_keys[i];
        int k1 = ent_num(1, 16U, 65536U, key.offset, 0);
        int k2 = ent_num(2, 16U, 65536U, key.offset, 0);

        if (key.type == 0) { // int
            int v1 = 0;
            int v2 = 0;
            int r1 = sceRegMgrGetInt(k1, &v1);
            int r2 = sceRegMgrGetInt(k2, &v2);

            if (r1 == 0 && r2 == 0) {
                if (v1 != v2) {
                    char line[256];
                    snprintf(line, sizeof(line), "DIFF int key=%s slot1=%d slot2=%d\n", key.name, v1, v2);
                    send_all(fd, line);
                }
            } else {
                char line[256];
                snprintf(line, sizeof(line), "ERR read int key=%s r1=%d r2=%d\n", key.name, r1, r2);
                send_all(fd, line);
            }
        }
        else if (key.type == 1) { // string
            char v1[2048];
            char v2[2048];
            memset(v1, 0, sizeof(v1));
            memset(v2, 0, sizeof(v2));

            int r1 = sceRegMgrGetStr(k1, v1, key.size > sizeof(v1) ? sizeof(v1) : key.size);
            int r2 = sceRegMgrGetStr(k2, v2, key.size > sizeof(v2) ? sizeof(v2) : key.size);

            if (r1 == 0 && r2 == 0) {
                if (strcmp(v1, v2) != 0) {
                    char line[256];
                    snprintf(line, sizeof(line), "DIFF str key=%s slot1=\"%s\" slot2=\"%s\"\n", key.name, v1, v2);
                    send_all(fd, line);
                }
            } else if (r1 != r2) {
                char line[256];
                snprintf(line, sizeof(line), "DIFF str key=%s presence slot1=%d slot2=%d\n", key.name, r1 == 0, r2 == 0);
                send_all(fd, line);
            }
        }
        else if (key.type == 2) { // bin
            uint8_t v1[2048];
            uint8_t v2[2048];
            memset(v1, 0, sizeof(v1));
            memset(v2, 0, sizeof(v2));

            int r1 = sceRegMgrGetBin(k1, v1, key.size > sizeof(v1) ? sizeof(v1) : key.size);
            int r2 = sceRegMgrGetBin(k2, v2, key.size > sizeof(v2) ? sizeof(v2) : key.size);

            if (r1 == 0 && r2 == 0) {
                if (memcmp(v1, v2, key.size > sizeof(v1) ? sizeof(v1) : key.size) != 0) {
                    char line[256];
                    snprintf(line, sizeof(line), "DIFF bin key=%s\n", key.name);
                    send_all(fd, line);
                }
            } else if (r1 != r2) {
                char line[256];
                snprintf(line, sizeof(line), "DIFF bin key=%s presence slot1=%d slot2=%d\n", key.name, r1 == 0, r2 == 0);
                send_all(fd, line);
            }
        }
    }

    send_all(fd, "END\n");
}

static void cmd_paired(int fd)
{
    send_all(fd, "OK PAIRED\n");

    for (int i = 1; i <= 32; i++) {
        int u_id = 0;
        int reg_key = 0;
        uint8_t aes[16];
        int client_type = 0;

        memset(aes, 0, sizeof(aes));

        int r1 = sceRegMgrGetInt(ent_num(i, 32U, 65536U, 1090584832U, 1092681984U), &u_id);
        int r2 = sceRegMgrGetInt(ent_num(i, 32U, 65536U, 1090585088U, 1092682240U), &reg_key);
        int r3 = sceRegMgrGetBin(ent_num(i, 32U, 65536U, 1090585344U, 1092682496U), aes, sizeof(aes));
        int r4 = sceRegMgrGetInt(ent_num(i, 32U, 65536U, 1090585600U, 1092682752U), &client_type);

        if (r1 == 0 && u_id != 0) {
            char line[256];
            snprintf(
                line,
                sizeof(line),
                "DEVICE index=%d user_id=%d/0x%08x regist_key=0x%08x client_type=%d aes=%02x%02x%02x%02x...\n",
                i,
                u_id,
                u_id,
                reg_key,
                client_type,
                aes[0], aes[1], aes[2], aes[3]
            );
            send_all(fd, line);
        }
    }

    for (int i = 1; i <= 16; i++) {
        int u_id = 0;
        int reg_key = 0;
        int client_type = 0;
        int r1 = sceRegMgrGetInt(ent_num(i, 16U, 65536U, 1115685120U, 1116733696U), &u_id);
        int r2 = sceRegMgrGetInt(ent_num(i, 16U, 65536U, 1115685376U, 1116733952U), &reg_key);
        int r4 = sceRegMgrGetInt(ent_num(i, 16U, 65536U, 1115685888U, 1116734464U), &client_type);

        if (r1 == 0 && u_id != 0) {
            char line[256];
            snprintf(
                line,
                sizeof(line),
                "DEVICE2 index=%d user_id=%d/0x%08x regist_key=0x%08x client_type=%d\n",
                i,
                u_id,
                u_id,
                reg_key,
                client_type
            );
            send_all(fd, line);
        }
    }

    for (int i = 1; i <= 16; i++) {
        int u_id = 0;
        int reg_key = 0;
        int client_type = 0;
        int r1 = sceRegMgrGetInt(ent_num(i, 16U, 65536U, 1132462336U, 1133510912U), &u_id);
        int r2 = sceRegMgrGetInt(ent_num(i, 16U, 65536U, 1132462592U, 1133511168U), &reg_key);
        int r4 = sceRegMgrGetInt(ent_num(i, 16U, 65536U, 1132463104U, 1133511680U), &client_type);

        if (r1 == 0 && u_id != 0) {
            char line[256];
            snprintf(
                line,
                sizeof(line),
                "DEVICE3 index=%d user_id=%d/0x%08x regist_key=0x%08x client_type=%d\n",
                i,
                u_id,
                u_id,
                reg_key,
                client_type
            );
            send_all(fd, line);
        }
    }

    send_all(fd, "END\n");
}


static void cmd_getkey(int fd, const char *type, const char *key_s)
{
    if (!type || !key_s) {
        send_all(fd, "ERR usage_GETKEY_type_id\nEND\n");
        return;
    }

    int key_id = atoi(key_s);
    if (key_id <= 0) {
        key_id = (int)strtol(key_s, NULL, 0);
    }

    if (!strcmp(type, "int")) {
        int val = 0;
        int ret = sceRegMgrGetInt(key_id, &val);
        char line[256];
        snprintf(line, sizeof(line), "OK GETKEY ret=0x%08x/dec=%d val=%d/0x%x\nEND\n", ret, ret, val, val);
        send_all(fd, line);
    } else if (!strcmp(type, "str")) {
        char val[2048];
        memset(val, 0, sizeof(val));
        int ret = sceRegMgrGetStr(key_id, val, sizeof(val));
        char line[2300];
        snprintf(line, sizeof(line), "OK GETKEY ret=0x%08x/dec=%d val=\"%s\"\nEND\n", ret, ret, val);
        send_all(fd, line);
    } else if (!strcmp(type, "bin")) {
        uint8_t val[2048];
        memset(val, 0, sizeof(val));
        int ret = sceRegMgrGetBin(key_id, val, sizeof(val));
        char line[4500];
        int pos = snprintf(line, sizeof(line), "OK GETKEY ret=0x%08x/dec=%d val=", ret, ret);
        if (ret == 0) {
            for (int i = 0; i < 64; i++) {
                pos += snprintf(line + pos, sizeof(line) - pos, "%02x", val[i]);
            }
        } else {
            pos += snprintf(line + pos, sizeof(line) - pos, "none");
        }
        snprintf(line + pos, sizeof(line) - pos, "\nEND\n");
        send_all(fd, line);
    } else {
        send_all(fd, "ERR invalid_type\nEND\n");
    }
}

static void cmd_setkey(int fd, const char *type, const char *key_s, const char *val_s)
{
    if (!type || !key_s || !val_s) {
        send_all(fd, "ERR usage_SETKEY_type_id_value\nEND\n");
        return;
    }

    int key_id = atoi(key_s);
    if (key_id <= 0) {
        key_id = (int)strtol(key_s, NULL, 0);
    }

    if (!strcmp(type, "int")) {
        int val = (int)strtol(val_s, NULL, 0);
        int ret = sceRegMgrSetInt(key_id, val);
        char line[256];
        snprintf(line, sizeof(line), "OK SETKEY ret=0x%08x/dec=%d\nEND\n", ret, ret);
        send_all(fd, line);
    } else if (!strcmp(type, "str")) {
        int ret = sceRegMgrSetStr(key_id, val_s, strlen(val_s) + 1);
        char line[256];
        snprintf(line, sizeof(line), "OK SETKEY ret=0x%08x/dec=%d\nEND\n", ret, ret);
        send_all(fd, line);
    } else if (!strcmp(type, "bin")) {
        uint8_t val[1024];
        size_t val_len = 0;
        const char *p = val_s;
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
        }
        while (*p && val_len < sizeof(val)) {
            unsigned int byte_val = 0;
            if (sscanf(p, "%2x", &byte_val) != 1) {
                break;
            }
            val[val_len++] = (uint8_t)byte_val;
            p += 2;
        }
        int ret = sceRegMgrSetBin(key_id, val, val_len);
        char line[256];
        snprintf(line, sizeof(line), "OK SETKEY ret=0x%08x/dec=%d len=%zu\nEND\n", ret, ret, val_len);
        send_all(fd, line);
    } else {
        send_all(fd, "ERR invalid_type\nEND\n");
    }
}

static void patch_str(unsigned char *buf, int offset, const char *str, int max_len) {
    memset(&buf[offset], 0, max_len);
    int len = strlen(str);
    if (len > max_len - 1) len = max_len - 1;
    memcpy(&buf[offset], str, len);
}

static int write_file(const char *path, const unsigned char *data, size_t len) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    ssize_t written = write(fd, data, len);
    close(fd);
    return (written == (ssize_t)len) ? 0 : -1;
}

static void write_registry_state(const unsigned char *cfg, uint32_t off) {
    int32_t val;

    if (cfg[0x108] != 0)
        sceRegMgrSetStr(125830656 + off, (const char*)&cfg[0x108], 65);
    sceRegMgrSetStr(125874188 + off, (const char*)&cfg[0x1AD], 17);
    sceRegMgrSetStr(125874183 + off, (const char*)&cfg[0x177], 17);
    sceRegMgrSetStr(125874190 + off, (const char*)&cfg[0x1BE], 3);
    sceRegMgrSetStr(125874191 + off, (const char*)&cfg[0x1C1], 6);
    sceRegMgrSetStr(125874192 + off, (const char*)&cfg[0x1C7], 36);

    memcpy(&val, &cfg[0x48], 4); sceRegMgrSetInt(125830144 + off, val);
    memcpy(&val, &cfg[0x4C], 4); sceRegMgrSetInt(125831424 + off, val);
    memcpy(&val, &cfg[0x50], 4); sceRegMgrSetInt(125831168 + off, val);
    memcpy(&val, &cfg[0x5C], 4); sceRegMgrSetInt(125832960 + off, val);
    memcpy(&val, &cfg[0x1F4], 4); sceRegMgrSetInt(125874194 + off, val);
    memcpy(&val, &cfg[0x1F8], 4); sceRegMgrSetInt(125874185 + off, val);
    memcpy(&val, &cfg[0x1FC], 4); sceRegMgrSetInt(125874186 + off, val);
    memcpy(&val, &cfg[0xA4], 4); sceRegMgrSetInt(125830912 + off, val);
    memcpy(&val, &cfg[0xB4], 4); sceRegMgrSetInt(125831936 + off, val);
    memcpy(&val, &cfg[0xD0], 4); sceRegMgrSetInt(125832704 + off, val);
    memcpy(&val, &cfg[0xD4], 4); sceRegMgrSetInt(125882625 + off, val);
    memcpy(&val, &cfg[0xDC], 4); sceRegMgrSetInt(125854723 + off, val);
    memcpy(&val, &cfg[0xF4], 4); sceRegMgrSetInt(125833216 + off, val);

    if (cfg[0x1100] != 0)
        sceRegMgrSetStr(125874189 + off, (const char*)&cfg[0x1100], 65);
    if (cfg[0x1141] != 0)
        sceRegMgrSetStr(125874193 + off, (const char*)&cfg[0x1141], 11);
    if (cfg[0x114C] != 0)
        sceRegMgrSetStr(125874195 + off, (const char*)&cfg[0x114C], 65);
}

static void cmd_fake_signin(int fd, const char *slot_s)
{
    if (!slot_s) {
        send_all(fd, "ERR usage_FAKESIGNIN_slot\nEND\n");
        return;
    }

    int target_slot = atoi(slot_s);
    if (target_slot < 1 || target_slot > ACCOUNT_NUMB_MAX) {
        send_all(fd, "ERR fake_signin invalid_slot\nEND\n");
        return;
    }

    if (target_slot == 1) {
        send_all(fd, "ERR fake_signin cannot_modify_slot_1_protection_active\nEND\n");
        return;
    }

    int current_slot = get_current_slot();
    if (current_slot < 1 || current_slot > ACCOUNT_NUMB_MAX) {
        send_all(fd, "ERR fake_signin failed_get_current_slot\nEND\n");
        return;
    }

    if (current_slot != target_slot) {
        char err_line[256];
        snprintf(err_line, sizeof(err_line), "ERR fake_signin target_slot_mismatch active_slot_is_%d_please_switch_user_on_ps5\nEND\n", current_slot);
        send_all(fd, err_line);
        return;
    }

    char userName[ACCOUNT_NAME_MAX];
    uint64_t account_id = 0;
    int user_id = 0;

    if (get_account_name(target_slot, userName) != 0 || userName[0] == '\0') {
        send_all(fd, "ERR fake_signin invalid_username\nEND\n");
        return;
    }

    if (get_account_id(target_slot, &account_id) != 0 || account_id == 0) {
        send_all(fd, "ERR fake_signin user_not_activated_aborting\nEND\n");
        return;
    }

    if (get_user_id(target_slot, &user_id) != 0) {
        send_all(fd, "ERR fake_signin failed_get_user_id\nEND\n");
        return;
    }

    unsigned char *cfg = malloc(sizeof(config_dat));
    if (!cfg) {
        send_all(fd, "ERR fake_signin out_of_memory\nEND\n");
        return;
    }
    memcpy(cfg, config_dat, sizeof(config_dat));

    struct { const char *country; const char *lang; const char *locale; } region = {"us", "en", "en-US"};
    int32_t sys_lang = -1;
    if (sceSystemServiceParamGetInt(1 /* LANG */, &sys_lang) == 0) {
        switch (sys_lang) {
            case  0: region = (typeof(region)){"jp","ja","ja-JP"}; break;
            case  1: region = (typeof(region)){"us","en","en-US"}; break;
            case  2: region = (typeof(region)){"fr","fr","fr-FR"}; break;
            case  3: region = (typeof(region)){"es","es","es-ES"}; break;
            case  4: region = (typeof(region)){"de","de","de-DE"}; break;
            case  5: region = (typeof(region)){"it","it","it-IT"}; break;
            case  6: region = (typeof(region)){"nl","nl","nl-NL"}; break;
            case  7: region = (typeof(region)){"pt","pt","pt-PT"}; break;
            case  8: region = (typeof(region)){"ru","ru","ru-RU"}; break;
            case  9: region = (typeof(region)){"kr","ko","ko-KR"}; break;
            case 10: region = (typeof(region)){"tw","zh","zh-TW"}; break;
            case 11: region = (typeof(region)){"cn","zh","zh-CN"}; break;
            case 12: region = (typeof(region)){"fi","fi","fi-FI"}; break;
            case 13: region = (typeof(region)){"se","sv","sv-SE"}; break;
            case 14: region = (typeof(region)){"dk","da","da-DK"}; break;
            case 15: region = (typeof(region)){"no","no","no-NO"}; break;
            case 16: region = (typeof(region)){"pl","pl","pl-PL"}; break;
            case 17: region = (typeof(region)){"br","pt","pt-BR"}; break;
            case 18: region = (typeof(region)){"gb","en","en-GB"}; break;
            case 19: region = (typeof(region)){"tr","tr","tr-TR"}; break;
            case 20: region = (typeof(region)){"mx","es","es-MX"}; break;
            case 21: region = (typeof(region)){"sa","ar","ar-SA"}; break;
            case 22: region = (typeof(region)){"ca","fr","fr-CA"}; break;
            case 23: region = (typeof(region)){"cz","cs","cs-CZ"}; break;
            case 24: region = (typeof(region)){"hu","hu","hu-HU"}; break;
            case 25: region = (typeof(region)){"gr","el","el-GR"}; break;
            case 26: region = (typeof(region)){"ro","ro","ro-RO"}; break;
            case 27: region = (typeof(region)){"th","th","th-TH"}; break;
            case 28: region = (typeof(region)){"vn","vi","vi-VN"}; break;
            case 29: region = (typeof(region)){"id","id","id-ID"}; break;
        }
    }

    patch_str(cfg, 0x1BE, region.country, 3);
    patch_str(cfg, 0x1C1, region.lang, 6);
    patch_str(cfg, 0x1C7, region.locale, 36);

    patch_str(cfg, 0x04, userName, 17);
    memcpy(&cfg[0x100], &account_id, 8);
    patch_str(cfg, 0x1AD, userName, 17);

    char np_email[65] = {0};
    snprintf(np_email, sizeof(np_email), "%s@a8.%s.np.playstation.net", userName, region.country);
    patch_str(cfg, 0x108, np_email, 65);
    patch_str(cfg, 0x1100, np_email, 65);

    char dir[256];
    snprintf(dir, sizeof(dir), "/system_data/priv/home/%x/np", user_id);
    mkdir(dir, 0755);

    char path[256];
    snprintf(path, sizeof(path), "/system_data/priv/home/%x/np/auth.dat", user_id);
    int r_auth = write_file(path, auth_dat, sizeof(auth_dat));

    snprintf(path, sizeof(path), "/system_data/priv/home/%x/config.dat", user_id);
    int r_config = write_file(path, cfg, sizeof(config_dat));

    uint32_t slot_off = (target_slot - 1) * 65536;
    write_registry_state(cfg, 0);
    if (target_slot > 1) {
        write_registry_state(cfg, slot_off);
    }

    free(cfg);

    char line[512];
    snprintf(
        line,
        sizeof(line),
        "OK FAKESIGNIN\n"
        "slot=%d\n"
        "user_id=0x%x\n"
        "username=\"%s\"\n"
        "account_id=0x%016" PRIx64 "\n"
        "country=\"%s\"\n"
        "lang=\"%s\"\n"
        "locale=\"%s\"\n"
        "write_auth_cfg=%d/%d\n"
        "reboot_recommended=1\n"
        "END\n",
        target_slot,
        user_id,
        userName,
        account_id,
        region.country,
        region.lang,
        region.locale,
        r_auth,
        r_config
    );
    send_all(fd, line);

    notifyf("ActRemoteLink: NP fake sign-in concluido com sucesso! Reinicie o PS5.");
}

static void handle_client(int fd)
{
    char buf[512];
    char *cmd;
    char *a1;
    char *a2;
    char *a3;

    memset(buf, 0, sizeof(buf));

    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);

    if (n <= 0) {
        return;
    }

    buf[n] = '\0';

    for (char *p = buf; *p; p++) {
        if (*p == '\r' || *p == '\n') {
            *p = '\0';
            break;
        }
    }

    cmd = strtok(buf, " \t");
    a1 = strtok(NULL, " \t");
    a2 = strtok(NULL, " \t");
    a3 = strtok(NULL, " \t");

    if (!cmd) {
        send_all(fd, "ERR empty_command\nEND\n");
        return;
    }

    if (!strcmp(cmd, "PING")) {
        send_all(fd, "OK PONG\nEND\n");
    } else if (!strcmp(cmd, "HELP")) {
        send_all(fd,
            "OK HELP\n"
            "PING\n"
            "LIST\n"
            "CURRENT\n"
            "B64 <hex_account_id>\n"
            "PREPARE_PIN\n"
            "PIN external_module\n"
            "SETID <slot> <hex_account_id>\n"
            "PAIRED\n"
            "COMPARE\n"
            "GETKEY <int|str|bin> <key_id>\n"
            "SETKEY <int|str|bin> <key_id> <value>\n"
            "FAKESIGNIN <slot>\n"
            "QUIT\n"
            "END\n"
        );
    } else if (!strcmp(cmd, "LIST")) {
        cmd_list(fd);
    } else if (!strcmp(cmd, "CURRENT")) {
        cmd_current(fd);
    } else if (!strcmp(cmd, "B64")) {
        cmd_b64(fd, a1);
    } else if (!strcmp(cmd, "PIN")) {
        cmd_pin(fd);
    } else if (!strcmp(cmd, "PREPARE_PIN")) {
        cmd_prepare_pin(fd);
    } else if (!strcmp(cmd, "SETID")) {
        cmd_setid(fd, a1, a2);
    } else if (!strcmp(cmd, "PAIRED")) {
        cmd_paired(fd);
    } else if (!strcmp(cmd, "COMPARE")) {
        cmd_compare(fd);
    } else if (!strcmp(cmd, "GETKEY")) {
        cmd_getkey(fd, a1, a2);
    } else if (!strcmp(cmd, "SETKEY")) {
        cmd_setkey(fd, a1, a2, a3);
    } else if (!strcmp(cmd, "FAKESIGNIN")) {
        cmd_fake_signin(fd, a1);
    } else if (!strcmp(cmd, "QUIT")) {
        send_all(fd, "OK BYE\nEND\n");
        close(fd);
        exit(0);
    } else {
        send_all(fd, "ERR unknown_command\nEND\n");
    }
}

int main(void)
{
    int server_fd;
    int yes = 1;
    struct sockaddr_in addr;

    notifyf("ActRemoteLink Agent started on port 31337");

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        notifyf("ActRemoteLink: socket failed");
            notifyf("ActRemoteLink: socket failed");
        return 1;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(AGENT_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        notifyf("ActRemoteLink: bind failed");
            notifyf("ActRemoteLink: bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 8) < 0) {
        notifyf("ActRemoteLink: listen failed");
            notifyf("ActRemoteLink: listen failed");
        close(server_fd);
        return 1;
    }

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);

        if (client_fd < 0) {
            continue;
        }

        handle_client(client_fd);
        close(client_fd);
    }

    return 0;
}
