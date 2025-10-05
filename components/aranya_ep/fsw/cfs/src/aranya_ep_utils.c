#include "cfe.h"
#include "cfe_evs.h"
#include "cfe_es.h"
#include "cfe_sb.h"
#include "cfe_msg.h"
#include "osapi.h"
#include "osapi-dir.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h> /* added for va_list */
#include <stdint.h>
#include <stdlib.h>

#include "aranya_ep_utils.h"
#include "aranya_ep_events.h"
#include "aranya_ep_msgids.h"
#include "aranya_ep_msg.h"
#include "aranya_ep_version.h"
#include "aranya_ep_perfids.h"

// Enable access to features through header
// #define ENABLE_ARANYA_AFC
// #define ENABLE_ARANYA_AQC
// #define ENABLE_ARANYA_PREVIEW
// #define ENABLE_ARANYA_EXPERIMENTAL 
#include "aranya-client.h" /* Aranya C API */

/* Persistent HK packet to avoid using a stack buffer that may be accessed
 * asynchronously by the software bus after ARANYA_EP_SendHousekeeping returns. */
static ARANYA_EP_HkTlm_t ARANYA_EP_HkPkt;

/* New: Persistent Onboard Announce packet for the same reason as HK */
static ARANYA_EP_OnboardAnnounceTlm_t ARANYA_EP_AnnouncePkt;

/* Simple runtime presence check for Aranya C API */
void ARANYA_EP_AranyaLibTest(void)
{
    AranyaExtError ext;
    memset(&ext, 0, sizeof(ext));
    size_t need = 0;
    (void)aranya_ext_error_msg(&ext, NULL, &need);
    CFE_EVS_SendEvent(ARANYA_EP_INIT_INF_EID, CFE_EVS_EventType_INFORMATION,
                      "Aranya C API presence check OK (ext msg size=%lu)", (unsigned long)need);
}

/* Brings up Aranya client */
bool ARANYA_EP_InitAranya(void)
{
    AranyaError rc;

    /* Track successful builder inits to decide if cleanup is safe */
    // TODO: cleanup all these inited checks, technically not necessary. See example.c
    // bool aqc_builder_inited        = false;
    bool client_cfg_builder_inited = false;

    /* Build client config */
    AranyaClientConfigBuilder client_cfg_builder;
    rc = aranya_client_config_builder_init(&client_cfg_builder);
    if (rc != ARANYA_ERROR_SUCCESS)
    {
        CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR, "Client config builder init failed (%d)",
                          rc);
        return false;
    }

    // OS_printf("Using Aranya daemon UDS path: %s\n", ARANYA_EP_DEFAULT_UDS_PATH);
    client_cfg_builder_inited = true;
    rc = aranya_client_config_builder_set_daemon_uds_path(&client_cfg_builder, ARANYA_EP_DEFAULT_UDS_PATH);
    if (rc != ARANYA_ERROR_SUCCESS)
    {
        CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR, "Set daemon UDS path failed (%d)", rc);
        if (client_cfg_builder_inited)
            aranya_client_config_builder_cleanup(&client_cfg_builder);
        return false;
    }

    // Consumed on build.
    AranyaClientConfig client_cfg;
    /* Use extended build to capture detailed error */
    AranyaExtError ext_err;
    memset(&ext_err, 0, sizeof(ext_err));
    rc = aranya_client_config_build_ext(&client_cfg_builder, &client_cfg, &ext_err);
    if (rc != ARANYA_ERROR_SUCCESS)
    {
        size_t err_len = 0;
        aranya_ext_error_msg(&ext_err, NULL, &err_len); /* query needed size */
        const char *fallback = "unknown";
        char       *buf      = NULL;
        if (err_len > 0 && err_len < 4096)
        {
            buf = (char *)malloc(err_len);
            if (buf)
            {
                if (aranya_ext_error_msg(&ext_err, buf, &err_len) != ARANYA_ERROR_SUCCESS)
                {
                    free(buf);
                    buf = NULL;
                }
            }
        }
        CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Client config build failed (%d): %s", rc, buf ? buf : fallback);
        if (buf)
            free(buf);
        return false;
    }

    /* Initialize client */
    memset(&ext_err, 0, sizeof(ext_err));
    rc = aranya_client_init_ext(&ARANYA_EP_App.Client, &client_cfg, &ext_err);
    if (rc != ARANYA_ERROR_SUCCESS)
    {
        size_t err_len = 0;
        aranya_ext_error_msg(&ext_err, NULL, &err_len); /* query needed size */
        const char *fallback = "unknown";
        char       *buf      = NULL;
        if (err_len > 0 && err_len < 4096) /* sanity cap */
        {
            buf = (char *)malloc(err_len);
            if (buf)
            {
                if (aranya_ext_error_msg(&ext_err, buf, &err_len) != ARANYA_ERROR_SUCCESS)
                {
                    free(buf);
                    buf = NULL;
                }
            }
        }

        CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR, "aranya_client_init failed (%d): %s", rc,
                          buf ? buf : fallback);
        if (buf)
            free(buf);

        return false;
    }

    AranyaDeviceId dev_id;
    rc = aranya_get_device_id(&ARANYA_EP_App.Client, &dev_id);
    if (rc == ARANYA_ERROR_SUCCESS)
    {
        char        dev_str[ARANYA_ID_STR_LEN] = {0};
        size_t      dev_str_len                = sizeof(dev_str);
        AranyaError rc2                        = aranya_id_to_str(&dev_id.id, dev_str, &dev_str_len);
        if (rc2 == ARANYA_ERROR_SUCCESS)
        {
            /* Cache device ID string for later telemetry use */
            (void)snprintf(ARANYA_EP_App.DeviceIdStr, sizeof(ARANYA_EP_App.DeviceIdStr), "%s", dev_str);

            CFE_EVS_SendEvent(ARANYA_EP_SOCKET_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "Connected to Aranya daemon; device ID=%s", dev_str);
        }
        else
        {
            ARANYA_EP_App.DeviceIdStr[0] = '\0';
            CFE_EVS_SendEvent(ARANYA_EP_SOCKET_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "Couldn't obtain dev ID. ERROR: %d", rc2);
        }
    }
    else
    {
        CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                          "aranya_get_device_id failed (%d); continuing", rc);
        aranya_client_cleanup(&ARANYA_EP_App.Client);

        return false;
    }

    /* Retrieve keybundle using non-NULL buffer pattern */
    ARANYA_EP_App.KeyBundleLen = 0; /* reset cached length before fetch */
    size_t   kb_len            = 1;
    uint8_t *kb                = (uint8_t *)calloc(kb_len, 1);
    if (kb == NULL)
    {
        CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR, "Keybundle initial alloc failed (len=%lu)",
                          (unsigned long)kb_len);
    }
    else
    {
        AranyaExtError kb_err;
        memset(&kb_err, 0, sizeof(kb_err));

        AranyaError krc = aranya_get_key_bundle_ext(&ARANYA_EP_App.Client, kb, &kb_len, &kb_err);
        if (krc == ARANYA_ERROR_BUFFER_TOO_SMALL)
        {
            /* Sanity cap to avoid unexpected huge allocations */
            if (kb_len == 0 || kb_len > (64 * 1024))
            {
                CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "Keybundle invalid required length %lu", (unsigned long)kb_len);
            }
            else
            {
                uint8_t *tmp = (uint8_t *)realloc(kb, kb_len);
                if (tmp == NULL)
                {
                    CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                                      "Keybundle realloc failed (len=%lu)", (unsigned long)kb_len);
                }
                else
                {
                    kb = tmp;
                    /* Ensure buffer is zeroed before receiving key bytes */
                    memset(kb, 0, kb_len);
                    memset(&kb_err, 0, sizeof(kb_err));
                    krc = aranya_get_key_bundle_ext(&ARANYA_EP_App.Client, kb, &kb_len, &kb_err);
                    if (krc == ARANYA_ERROR_SUCCESS)
                    {
                        /* Cache keybundle length upon successful retrieval */
                        ARANYA_EP_App.KeyBundleLen = (uint32)kb_len;

                        /* Save keybundle to /data/aranya/keybundle.bin via OSAL */
                        (void)OS_mkdir("/data/aranya", OS_DEFAULT_FILE_PERMISSIONS); /* ok if it already exists */

                        const char *kb_path = "/data/aranya/keybundle.bin";
                        osal_id_t   fd      = OS_OBJECT_ID_UNDEFINED;
                        int32       rc_open =
                            OS_OpenCreate(&fd, kb_path, OS_FILE_FLAG_CREATE | OS_FILE_FLAG_TRUNCATE, OS_READ_WRITE);
                        if (rc_open == OS_SUCCESS)
                        {
                            int32 rc_wr = OS_write(fd, kb, (size_t)kb_len);
                            if (rc_wr == (int32)kb_len)
                            {
                                CFE_EVS_SendEvent(ARANYA_EP_SOCKET_INF_EID, CFE_EVS_EventType_INFORMATION,
                                                  "Keybundle saved to %s (%lu bytes)", kb_path, (unsigned long)kb_len);
                            }
                            else
                            {
                                CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                                                  "Keybundle write failed (%ld/%lu) to %s", (long)rc_wr,
                                                  (unsigned long)kb_len, kb_path);
                            }
                            (void)OS_close(fd);
                        }
                        else
                        {
                            CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                                              "Keybundle open/create failed rc=%ld path=%s", (long)rc_open, kb_path);
                        }
                    }
                    else
                    {
                        size_t elen = 0;
                        aranya_ext_error_msg(&kb_err, NULL, &elen);
                        char *ebuf = NULL;
                        if (elen > 0 && elen < 4096)
                        {
                            ebuf = (char *)malloc(elen);
                            if (ebuf)
                            {
                                if (aranya_ext_error_msg(&kb_err, ebuf, &elen) != ARANYA_ERROR_SUCCESS)
                                {
                                    free(ebuf);
                                    ebuf = NULL;
                                }
                            }
                        }
                        CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                                          "Keybundle fetch failed (%d): %s", krc, ebuf ? ebuf : "unknown");
                        if (ebuf)
                            free(ebuf);
                    }
                }
            }
        }
        else if (krc == ARANYA_ERROR_SUCCESS)
        {
            /* Cache keybundle length when small initial buffer was enough */
            ARANYA_EP_App.KeyBundleLen = (uint32)kb_len;

            /* Edge case: small initial buffer was enough; save directly */
            (void)OS_mkdir("/data/aranya", OS_DEFAULT_FILE_PERMISSIONS);
            const char *kb_path = "/data/aranya/keybundle.bin";
            osal_id_t   fd      = OS_OBJECT_ID_UNDEFINED;
            int32 rc_open = OS_OpenCreate(&fd, kb_path, OS_FILE_FLAG_CREATE | OS_FILE_FLAG_TRUNCATE, OS_READ_WRITE);
            if (rc_open == OS_SUCCESS)
            {
                int32 rc_wr = OS_write(fd, kb, (size_t)kb_len);
                if (rc_wr == (int32)kb_len)
                {
                    CFE_EVS_SendEvent(ARANYA_EP_SOCKET_INF_EID, CFE_EVS_EventType_INFORMATION,
                                      "Keybundle saved to %s (%lu bytes)", kb_path, (unsigned long)kb_len);
                }
                else
                {
                    CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                                      "Keybundle write failed (%ld/%lu) to %s", (long)rc_wr, (unsigned long)kb_len,
                                      kb_path);
                }
                (void)OS_close(fd);
            }
            else
            {
                CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "Keybundle open/create failed rc=%ld path=%s", (long)rc_open, kb_path);
            }
        }
        else
        {
            size_t elen = 0;
            aranya_ext_error_msg(&kb_err, NULL, &elen);
            char *ebuf = NULL;
            if (elen > 0 && elen < 4096)
            {
                ebuf = (char *)malloc(elen);
                if (ebuf)
                {
                    if (aranya_ext_error_msg(&kb_err, ebuf, &elen) != ARANYA_ERROR_SUCCESS)
                    {
                        free(ebuf);
                        ebuf = NULL;
                    }
                }
            }
            CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR, "Keybundle fetch failed (%d): %s", krc,
                              ebuf ? ebuf : "unknown");
            if (ebuf)
                free(ebuf);
        }

        if (kb)
            free(kb);
    }

    ARANYA_EP_App.ClientInitialized = true;

    /* Send onboarding announcement once, now that client init succeeded */
    ARANYA_EP_SendOnboardAnnounce();

    return true;
}

/* Initialize app */
int32 ARANYA_EP_Init(void)
{
    int32 status;

    memset(&ARANYA_EP_App, 0, sizeof(ARANYA_EP_App));
    ARANYA_EP_App.DestMsgId = CFE_SB_INVALID_MSG_ID;

    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
        return status;

    ARANYA_EP_AranyaLibTest();

    status = CFE_SB_CreatePipe(&ARANYA_EP_App.CmdPipeId, ARANYA_EP_PIPE_DEPTH, "ARANYA_EP_CMD_PIPE");
    if (status != CFE_SUCCESS)
        return status;

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ARANYA_EP_CMD_MID), ARANYA_EP_App.CmdPipeId);
    if (status != CFE_SUCCESS)
        return status;

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ARANYA_EP_SEND_HK_MID), ARANYA_EP_App.CmdPipeId);
    if (status != CFE_SUCCESS)
        return status;

    ARANYA_EP_App.RunStatus = CFE_ES_RunStatus_APP_RUN;
    /* Copy default UDS path safely ensuring null-termination */
    (void)snprintf(ARANYA_EP_App.UdsPath, sizeof(ARANYA_EP_App.UdsPath), "%s", ARANYA_EP_DEFAULT_UDS_PATH);
    ARANYA_EP_App.UdsPath[sizeof(ARANYA_EP_App.UdsPath) - 1] = '\0';

    if (ARANYA_EP_InitAranya())
    {
        CFE_EVS_SendEvent(ARANYA_EP_INIT_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "ARANYA_EP v%u.%u.%u.%u initialized, UDS:%s", ARANYA_EP_MAJOR_VERSION,
                          ARANYA_EP_MINOR_VERSION, ARANYA_EP_REVISION, ARANYA_EP_MISSION_REV, ARANYA_EP_App.UdsPath);
        OS_printf("[ARANYA_EP] Initialized. Daemon UDS=%s\n", ARANYA_EP_App.UdsPath);
    }
    else
    {
        CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Aranya client init failed; FORWARD will deny");
        OS_printf("[ARANYA_EP] Aranya client init failed; FORWARD will deny\n");
    }

    return CFE_SUCCESS;
}

/* Housekeeping */
void ARANYA_EP_SendHousekeeping(void)
{
    /* Use persistent packet to avoid dangling pointer after return */
    ARANYA_EP_HkTlm_t *hk = &ARANYA_EP_HkPkt;
    memset(hk, 0, sizeof(*hk));
    CFE_MSG_Init(CFE_MSG_PTR(hk->TlmHeader), CFE_SB_ValueToMsgId(ARANYA_EP_HK_TLM_MID), sizeof(*hk));

    hk->CmdCounter      = ARANYA_EP_App.CmdCounter;
    hk->ErrCounter      = ARANYA_EP_App.ErrCounter;
    hk->AuthorizedCount = ARANYA_EP_App.AuthorizedCount;
    hk->DeniedCount     = ARANYA_EP_App.DeniedCount;
    hk->LastAuthResult  = ARANYA_EP_App.LastAuthResult;
    hk->DestMsgIdVal    = CFE_SB_MsgIdToValue(ARANYA_EP_App.DestMsgId);

    int32 tx_status = CFE_SB_TransmitMsg((CFE_MSG_Message_t *)hk, true);
    if (tx_status == CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(ARANYA_EP_HK_SENT_EID, CFE_EVS_EventType_INFORMATION,
                          "HK telemetry sent: CmdCnt=%u ErrCnt=%u DestMID=0x%08lX", hk->CmdCounter, hk->ErrCounter,
                          (unsigned long)hk->DestMsgIdVal);
    }

    // TODO: remove, here temporarily for debugging
    ARANYA_EP_SendOnboardAnnounce();
}

/* New: Send Onboard Announce telemetry once (or when desired) */
void ARANYA_EP_SendOnboardAnnounce(void)
{
    ARANYA_EP_OnboardAnnounceTlm_t *onb_announcement = &ARANYA_EP_AnnouncePkt;
    memset(onb_announcement, 0, sizeof(*onb_announcement));
    CFE_MSG_Init(CFE_MSG_PTR(onb_announcement->TlmHeader), CFE_SB_ValueToMsgId(ARANYA_EP_ONBOARD_ANNOUNCE_TLM_MID),
                 sizeof(*onb_announcement));

    /* Device ID string from cached AppData value */
    if (ARANYA_EP_App.DeviceIdStr[0] != '\0')
    {
        (void)snprintf(onb_announcement->DeviceId, sizeof(onb_announcement->DeviceId), "%s", ARANYA_EP_App.DeviceIdStr);
    }

    /* Use cached keybundle metadata collected in InitAranya */
    onb_announcement->KeyBundleLen     = ARANYA_EP_App.KeyBundleLen;
    onb_announcement->KeyBundleHash[0] = '\0'; /* TODO: populate when available */

    int32 tx_status = CFE_SB_TransmitMsg((CFE_MSG_Message_t *)onb_announcement, true);
    if (tx_status == CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(ARANYA_EP_HK_SENT_EID, CFE_EVS_EventType_INFORMATION, "ONBOARDING telemetry sent.");
    }
}

bool ARANYA_EP_ValidateUdsPath(const char *Path)
{
    if (Path == NULL || Path[0] != '/')
        return false;
    for (const char *p = Path; *p != '\0'; ++p)
    {
        if ((unsigned char)*p < 0x20)
            return false;
    }
    return true;
}

int32 ARANYA_EP_VerifyCmdLength(CFE_MSG_Message_t *MsgPtr, size_t Expected)
{
    size_t actual = 0;
    CFE_MSG_GetSize(MsgPtr, &actual);
    if (actual != Expected)
    {
        CFE_SB_MsgId_t    mid;
        CFE_MSG_FcnCode_t fc;
        CFE_MSG_GetMsgId(MsgPtr, &mid);
        CFE_MSG_GetFcnCode(MsgPtr, &fc);
        CFE_EVS_SendEvent(ARANYA_EP_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Length error MID=0x%08lX FC=%u Got=%lu Exp=%lu", (unsigned long)CFE_SB_MsgIdToValue(mid),
                          (unsigned)fc, (unsigned long)actual, (unsigned long)Expected);
        ARANYA_EP_App.ErrCounter++;
        return CFE_SB_BAD_ARGUMENT;
    }
    return CFE_SUCCESS;
}

int32 ARANYA_EP_MountAndList(const char *dev_name, const char *mount_point, const char *dir_path)
{
    int32       status;
    osal_id_t   dir_id;
    os_dirent_t dirent;
    bool        mounted = false;

    if (dev_name && mount_point)
    {
        /* Heuristic: if dev_name looks like a host absolute path, skip OS_mount attempt */
        if (dev_name[0] == '/')
        {
            OS_printf("ARANYA_EP: Skipping OS_mount; '%s' looks like a host path (dir='%s')\n", dev_name, dir_path);
        }
        else
        {
            OS_printf("ARANYA_EP: Mounting %s at %s\n", dev_name, mount_point);
            status = OS_mount(dev_name, mount_point);
            if (status != OS_SUCCESS)
            {
                OS_printf("ARANYA_EP: OS_mount failed dev=%s mp=%s rc=%ld\n", dev_name, mount_point, (long)status);
                CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "FS mount failed dev=%s mp=%s rc=%ld", dev_name, mount_point, (long)status);
                return status;
            }
            mounted = true;
        }
    }

    status = OS_DirectoryOpen(&dir_id, dir_path);
    if (status != OS_SUCCESS)
    {
        OS_printf("ARANYA_EP: Directory open failed path=%s rc=%ld\n", dir_path, (long)status);
        CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR, "Directory open failed path=%s rc=%ld",
                          dir_path, (long)status);
        if (mounted)
            OS_unmount(mount_point);
        return status;
    }

    OS_printf("ARANYA_EP: Listing dir: %s \n", dir_path);
    while ((status = OS_DirectoryRead(dir_id, &dirent)) == OS_SUCCESS)
    {
        OS_printf("[ARANYA_EP] ENTRY: %s\n", OS_DIRENTRY_NAME(dirent));
    }
    if (status == OS_ERROR) /* OS_ERROR is normal end-of-dir per OSAL spec */
    {
        OS_printf("Directory read ended rc=%ld\n", (long)status);
    }

    OS_DirectoryClose(dir_id);

    if (mounted)
    {
        status = OS_unmount(mount_point);
        if (status != OS_SUCCESS)
        {
            CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR, "Unmount failed mp=%s rc=%ld",
                              mount_point, (long)status);
        }
    }
    return OS_SUCCESS;
}

/* Simple wrapper when only listing an existing directory */
int32 ARANYA_EP_ListDir(const char *dir_path)
{
    return ARANYA_EP_MountAndList(NULL, NULL, dir_path);
}