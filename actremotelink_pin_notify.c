#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <inttypes.h>

#define SCE_REGMGR_ENT_KEY_REMOTEPLAY_rp_enable 1098973184
#define ACCOUNT_NUMB_MAX 16

typedef struct notify_request {
    char useless1[45];
    char message[3075];
} notify_request_t;

int sceKernelSendNotificationRequest(int, notify_request_t*, size_t, int);

int sceUserServiceInitialize(void *);
int sceUserServiceGetForegroundUser(int *);

int sceRegMgrGetInt(int, int *);
int sceRegMgrGetBin(int, void *, size_t);
int sceRegMgrSetInt(int, int);

int sceRemoteplayInitialize(void *, size_t);
int sceRemoteplayGeneratePinCode(uint32_t *);
int sceRemoteplayConfirmDeviceRegist(int *, int *);
int sceRemoteplayNotifyPinCodeError(int);

static void notify_msg(const char *msg)
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
    return ent_num(slot, 16, 65536, 125829376, 127140096);
}

static int key_account_id(int slot)
{
    return ent_num(slot, 16, 65536, 125830400, 127141120);
}

static int key_user_rp_enable(int slot)
{
    return ent_num(slot, 16, 65536, 125859841, 127170561);
}

static int get_current_slot(void)
{
    int fg_user = 0;
    int user_id = 0;

    if (sceUserServiceInitialize(0) != 0) {
        return -1;
    }

    if (sceUserServiceGetForegroundUser(&fg_user) != 0) {
        return -1;
    }

    for (int slot = 1; slot <= ACCOUNT_NUMB_MAX; slot++) {
        user_id = 0;

        if (sceRegMgrGetInt(key_user_id(slot), &user_id) == 0) {
            if (user_id == fg_user) {
                return slot;
            }
        }
    }

    return -1;
}

static void base64_encode_8(const uint8_t in[8], char out[13])
{
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int i = 0;
    int j = 0;

    while (i < 8) {
        int remain = 8 - i;

        uint8_t a = in[i++];
        uint8_t b = remain > 1 ? in[i++] : 0;
        uint8_t c = remain > 2 ? in[i++] : 0;

        out[j++] = tbl[(a >> 2) & 0x3f];
        out[j++] = tbl[((a & 0x03) << 4) | ((b >> 4) & 0x0f)];
        out[j++] = remain > 1 ? tbl[((b & 0x0f) << 2) | ((c >> 6) & 0x03)] : '=';
        out[j++] = remain > 2 ? tbl[c & 0x3f] : '=';
    }

    out[12] = '\0';
}

int main(void)
{
    int ret = 0;
    int slot = -1;

    int rp_enable = 0;
    int pair_stat = 0;
    int pair_err = 0;

    uint64_t account_id = 0;
    char account_b64[13];

    uint32_t pin = 0;
    char msg[512];

    time_t end_time;
    time_t now;
    time_t last_notify_ts = 0;

    memset(account_b64, 0, sizeof(account_b64));
    memset(msg, 0, sizeof(msg));

    printf("ActRemoteLink: initializing Remote Play...\n");

    ret = sceRemoteplayInitialize(0, 0);
    if (ret != 0) {
        snprintf(
            msg,
            sizeof(msg),
            "ActRemoteLink ERROR\nsceRemoteplayInitialize: 0x%x",
            ret
        );
        notify_msg(msg);
        printf("%s\n", msg);
        return -1;
    }

    if (sceRegMgrGetInt(SCE_REGMGR_ENT_KEY_REMOTEPLAY_rp_enable, &rp_enable) != 0) {
        notify_msg("ActRemoteLink ERROR\nFailed to read rp_enable");
        return -1;
    }

    if (rp_enable != 1) {
        if (sceRegMgrSetInt(SCE_REGMGR_ENT_KEY_REMOTEPLAY_rp_enable, 1) != 0) {
            notify_msg("ActRemoteLink ERROR\nFailed to enable Remote Play");
            return -1;
        }
    }

    slot = get_current_slot();
    if (slot < 1 || slot > ACCOUNT_NUMB_MAX) {
        notify_msg("ActRemoteLink ERROR\nFailed to identify current user");
        return -1;
    }

    int user_rp_enable = 0;
    if (sceRegMgrGetInt(key_user_rp_enable(slot), &user_rp_enable) == 0) {
        if (user_rp_enable != 1) {
            sceRegMgrSetInt(key_user_rp_enable(slot), 1);
        }
    }

    if (sceRegMgrGetBin(key_account_id(slot), &account_id, sizeof(account_id)) != 0) {
        notify_msg("ActRemoteLink ERROR\nFailed to read account_id");
        return -1;
    }

    base64_encode_8((uint8_t *)&account_id, account_b64);

    /*
     * Cancel any previous PIN if one exists.
     */
    sceRemoteplayNotifyPinCodeError(1);

    ret = sceRemoteplayGeneratePinCode(&pin);
    if (ret != 0) {
        snprintf(
            msg,
            sizeof(msg),
            "ActRemoteLink ERROR\nsceRemoteplayGeneratePinCode: 0x%x",
            ret
        );
        notify_msg(msg);
        printf("%s\n", msg);
        return -1;
    }

    snprintf(
        msg,
        sizeof(msg),
        "ActRemoteLink PIN\nPIN: %04u %04u\nID: %s",
        pin / 10000,
        pin % 10000,
        account_b64
    );

    notify_msg(msg);
    last_notify_ts = time(0);

    printf("OK PIN\n");
    printf("slot=%d\n", slot);
    printf("pin=%04u %04u\n", pin / 10000, pin % 10000);
    printf("pin_raw=%08u\n", pin);
    printf("account_id_hex=0x%016" PRIx64 "\n", account_id);
    printf("account_id_base64=%s\n", account_b64);
    printf("timeout=300\n");
    printf("END\n");

    /*
     * Keep the payload alive while pairing is active.
     * Resend the notification every 30 seconds.
     */
    end_time = time(0) + 300;

    while (1) {
        now = time(0);

        if (now >= end_time) {
            sceRemoteplayNotifyPinCodeError(1);
            notify_msg("ActRemoteLink\nPairing expired");
            return 0;
        }

        int remaining = (int)(end_time - now);

        pair_stat = 0;
        pair_err = 0;

        ret = sceRemoteplayConfirmDeviceRegist(&pair_stat, &pair_err);
        if (ret == 0) {
            if (pair_stat == 2) {
                notify_msg("ActRemoteLink\nPairing completed successfully");
                printf("PAIRING SUCCESS\n");
                return 0;
            }

            /*
             * Resend the PIN only every 60 seconds.
             * The first notification is sent immediately after the PIN is generated.
             */
            if ((now - last_notify_ts) >= 60) {
                snprintf(
                    msg,
                    sizeof(msg),
                    "ActRemoteLink PIN\nPIN: %04u %04u\nID: %s\nTime: %d s",
                    pin / 10000,
                    pin % 10000,
                    account_b64,
                    remaining
                );

                notify_msg(msg);
                last_notify_ts = now;
            }

            if (pair_stat == 3) {
                if (pair_err == 0x80FC1047) {
                    notify_msg("ActRemoteLink\nError: incorrect PIN");
                    printf("PAIRING FAILED invalid_pin\n");
                } else if (pair_err == 0x80FC1040) {
                    notify_msg("ActRemoteLink\nError: incorrect Account ID");
                    printf("PAIRING FAILED invalid_account_id\n");
                } else {
                    snprintf(
                        msg,
                        sizeof(msg),
                        "ActRemoteLink\nPairing error: 0x%x",
                        pair_err
                    );
                    notify_msg(msg);
                    printf("PAIRING FAILED 0x%x\n", pair_err);
                }

                return -1;
            }
        }

        sleep(1);
    }

    return 0;
}
