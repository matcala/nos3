#include "cfe.h"
#include "cfe_evs.h"
#include "cfe_es.h"
#include "cfe_sb.h"
#include "cfe_msg.h"
#include "osapi.h"

#include "aranya_ep_app.h"

/* Global app data - defined here, declared extern in header */
ARANYA_EP_AppData_t ARANYA_EP_App;

/* Entry point */
void ARANYA_EP_AppMain(void)
{
    int32 status = CFE_SUCCESS;
    CFE_ES_PerfLogEntry(ARANYA_EP_MAIN_TASK_PERF_ID);

    status = ARANYA_EP_Init();
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("[ARANYA_EP] App init failed: 0x%08lX\n", (unsigned long)status);
        CFE_ES_PerfLogExit(ARANYA_EP_MAIN_TASK_PERF_ID);
        /* Exit with failure */
        CFE_ES_ExitApp(CFE_ES_RunStatus_APP_ERROR);
        return;
    }

    /* Main loop */
    for (;;)
    {
        CFE_ES_PerfLogEntry(ARANYA_EP_MAIN_TASK_PERF_ID);

        status = CFE_SB_ReceiveBuffer(&ARANYA_EP_App.SbBufPtr, ARANYA_EP_App.CmdPipeId, CFE_SB_PEND_FOREVER);

        if (status == CFE_SUCCESS)
        {
            ARANYA_EP_ProcessCommand();
        }
        else
        {
            CFE_EVS_SendEvent(ARANYA_EP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "CFE_SB_ReceiveBuffer() failed: 0x%08lX", (unsigned long)status);
        }

        CFE_ES_PerfLogExit(ARANYA_EP_MAIN_TASK_PERF_ID);
    }
}

/* Initialize app */
int32 ARANYA_EP_Init(void)
{
    int32 status;

    memset(&ARANYA_EP_App, 0, sizeof(ARANYA_EP_App));
    ARANYA_EP_App.DestMsgId = CFE_SB_INVALID_MSG_ID;

    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    if (status != CFE_SUCCESS)
    {
        return status;
    }

    /* Create and subscribe command pipe */
    status = CFE_SB_CreatePipe(&ARANYA_EP_App.CmdPipeId, ARANYA_EP_PIPE_DEPTH, "ARANYA_EP_CMD_PIPE");
    if (status != CFE_SUCCESS) return status;

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ARANYA_EP_CMD_MID), ARANYA_EP_App.CmdPipeId);
    if (status != CFE_SUCCESS) return status;

    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(ARANYA_EP_SEND_HK_MID), ARANYA_EP_App.CmdPipeId);
    if (status != CFE_SUCCESS) return status;

    // TODO: check paths???
    strncpy(ARANYA_EP_App.UdsPath, "/ram/aranya/aranya_ep.sock", sizeof(ARANYA_EP_App.UdsPath)-1);
    OS_mkdir("/ram/aranya"); /* Best-effort ensure dir exists */

    /* Initialize Aranya client (daemon must be running) */
    if (ARANYA_EP_InitAranya())
    {
        CFE_EVS_SendEvent(ARANYA_EP_INIT_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "ARANYA_EP v%u.%u.%u.%u initialized, UDS:%s",
                          ARANYA_EP_MAJOR_VERSION, ARANYA_EP_MINOR_VERSION,
                          ARANYA_EP_REVISION, ARANYA_EP_MISSION_REV,
                          ARANYA_EP_App.UdsPath);
        OS_printf("[ARANYA_EP] Initialized. UDS=%s\n", ARANYA_EP_App.UdsPath);
    }
    else
    {
        CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Aranya client init failed; FORWARD will deny");
        OS_printf("[ARANYA_EP] Aranya client init failed; FORWARD will deny\n");
    }

    return CFE_SUCCESS;
}

/* Brings up Aranya client */
bool ARANYA_EP_InitAranya(void)
{
    /* Many Aranya versions accept a default/zeroed config.
     * If your version requires setting a socket path explicitly, do it here.
     * (e.g., AranyaClientConfig cfg = { .socket_path = ARANYA_EP_App.UdsPath };)
     */
    struct AranyaClientConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    struct AranyaExtError ext = {0};
    AranyaError rc = aranya_client_init_ext(&ARANYA_EP_App.ArClient, &cfg, &ext);
    if (rc != ARANYA_ERROR_SUCCESS)
    {
        size_t n = 0;
        (void)aranya_ext_error_msg(&ext, NULL, &n);
        char *buf = (n > 0) ? malloc(n) : NULL;
        if (buf)
        {
            (void)aranya_ext_error_msg(&ext, buf, &n);
            CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                              "aranya_client_init_ext error: %s", buf);
            free(buf);
        }
        return false;
    }

    /* Optional: log device id */
    struct AranyaDeviceId devid;
    rc = aranya_get_device_id_ext(&ARANYA_EP_App.ArClient, &devid, &ext);
    if (rc == ARANYA_ERROR_SUCCESS)
    {
        CFE_EVS_SendEvent(ARANYA_EP_SOCKET_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "Connected to Aranya daemon; device id obtained");
    }
    else
    {
        CFE_EVS_SendEvent(ARANYA_EP_ARANYA_ERR_EID, CFE_EVS_EventType_ERROR,
                          "aranya_get_device_id failed; continuing");
    }

    ARANYA_EP_App.ArClientInit = true;
    return true;
}

/* Basic “authorize” placeholder:
 *  - If Aranya is up, returns true (prototype default-policy)
 *  - You will replace with a real on-graph authorization flow.
 */
bool ARANYA_EP_AuthorizeForward(void)
{
    if (!ARANYA_EP_App.ArClientInit)
    {
        ARANYA_EP_App.LastAuthResult = ARANYA_EP_AUTH_ERROR;
        return false;
    }

    /* TODO: Here is where you call your on-graph authorization query.
     * E.g., check role/label/policy predicates for the requested action.
     * For MVP: assume authorized if client is live.
     */
    ARANYA_EP_App.LastAuthResult = ARANYA_EP_AUTH_ALLOWED;
    return true;
}

/* Process incoming SB message(s) */
void ARANYA_EP_ProcessCommand(void)
{
    CFE_SB_MsgId_t   msgid = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t fcode = 0;

    CFE_MSG_GetMsgId(&ARANYA_EP_App.SbBufPtr->Msg, &msgid);

    if (CFE_SB_MsgId_Equal(msgid, CFE_SB_ValueToMsgId(ARANYA_EP_CMD_MID)))
    {
        CFE_MSG_GetFcnCode(&ARANYA_EP_App.SbBufPtr->Msg, &fcode);
        ARANYA_EP_ProcessGroundCommand(fcode);
    }
    else if (CFE_SB_MsgId_Equal(msgid, CFE_SB_ValueToMsgId(ARANYA_EP_SEND_HK_MID)))
    {
        ARANYA_EP_SendHousekeeping();
    }
    else
    {
        ARANYA_EP_App.ErrCounter++;
        CFE_EVS_SendEvent(ARANYA_EP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid MsgId: 0x%08lX",
                          (unsigned long)CFE_SB_MsgIdToValue(msgid));
    }
}

/* Process command codes */
void ARANYA_EP_ProcessGroundCommand(CFE_MSG_FcnCode_t FcnCode)
{
    switch (FcnCode)
    {
    case ARANYA_EP_NOOP_CC:
        ARANYA_EP_App.CmdCounter++;
        CFE_EVS_SendEvent(ARANYA_EP_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "NOOP received. v%u.%u.%u.%u",
                          ARANYA_EP_MAJOR_VERSION, ARANYA_EP_MINOR_VERSION,
                          ARANYA_EP_REVISION, ARANYA_EP_MISSION_REV);
        break;

    case ARANYA_EP_RESET_CC:
        ARANYA_EP_App.CmdCounter     = 0;
        ARANYA_EP_App.ErrCounter     = 0;
        ARANYA_EP_App.AuthorizedCount= 0;
        ARANYA_EP_App.DeniedCount    = 0;
        ARANYA_EP_App.LastAuthResult = ARANYA_EP_AUTH_UNKNOWN;
        CFE_EVS_SendEvent(ARANYA_EP_RESET_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "Reset counters");
        break;

    case ARANYA_EP_SET_DEST_CC:
    {
        ARANYA_EP_SetDestCmd_t *cmd = (ARANYA_EP_SetDestCmd_t *) ARANYA_EP_App.SbBufPtr;
        /* Basic length check */
        size_t expect = sizeof(ARANYA_EP_SetDestCmd_t);
        size_t actual = 0;
        CFE_MSG_GetSize(&ARANYA_EP_App.SbBufPtr->Msg, &actual);
        if (actual != expect)
        {
            ARANYA_EP_App.ErrCounter++;
            CFE_EVS_SendEvent(ARANYA_EP_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
                              "SET_DEST length err: got %lu exp %lu",
                              (unsigned long)actual, (unsigned long)expect);
            break;
        }
        ARANYA_EP_App.DestMsgId = CFE_SB_ValueToMsgId(cmd->DestMsgIdVal);
        ARANYA_EP_App.DestSet   = true;
        ARANYA_EP_App.CmdCounter++;
        CFE_EVS_SendEvent(ARANYA_EP_SETDEST_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "Dest MID set to 0x%08lX", (unsigned long)cmd->DestMsgIdVal);
        break;
    }

    case ARANYA_EP_SET_UDS_CC:
    {
        ARANYA_EP_SetUdsCmd_t *cmd = (ARANYA_EP_SetUdsCmd_t *) ARANYA_EP_App.SbBufPtr;
        size_t actual = 0;
        CFE_MSG_GetSize(&ARANYA_EP_App.SbBufPtr->Msg, &actual);
        if (actual != sizeof(ARANYA_EP_SetUdsCmd_t))
        {
            ARANYA_EP_App.ErrCounter++;
            CFE_EVS_SendEvent(ARANYA_EP_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
                              "SET_UDS length err: got %lu",
                              (unsigned long)actual);
            break;
        }
        strncpy(ARANYA_EP_App.UdsPath, cmd->UdsPath, sizeof(ARANYA_EP_App.UdsPath)-1);
        ARANYA_EP_App.UdsPath[sizeof(ARANYA_EP_App.UdsPath)-1] = '\0';
        CFE_EVS_SendEvent(ARANYA_EP_SOCKET_INF_EID, CFE_EVS_EventType_INFORMATION,
                          "UDS path updated: %s (effective next reinit)", ARANYA_EP_App.UdsPath);
        ARANYA_EP_App.CmdCounter++;
        break;
    }

    case ARANYA_EP_FORWARD_CC:
    {
        ARANYA_EP_ForwardCmd_t *cmd = (ARANYA_EP_ForwardCmd_t *) ARANYA_EP_App.SbBufPtr;
        size_t actual = 0;
        CFE_MSG_GetSize(&ARANYA_EP_App.SbBufPtr->Msg, &actual);
        /* Sanity bounds: header + len <= max */
        if (actual < sizeof(ARANYA_EP_ForwardCmd_t) - ARANYA_EP_MAX_FORWARD_LEN ||
            cmd->PayloadLen > ARANYA_EP_MAX_FORWARD_LEN ||
            (actual != (sizeof(ARANYA_EP_ForwardCmd_t) - ARANYA_EP_MAX_FORWARD_LEN + cmd->PayloadLen)))
        {
            ARANYA_EP_App.ErrCounter++;
            CFE_EVS_SendEvent(ARANYA_EP_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
                              "FORWARD length err: payload_len=%u, msg_size=%lu",
                              cmd->PayloadLen, (unsigned long)actual);
            break;
        }

        if (!ARANYA_EP_App.DestSet)
        {
            ARANYA_EP_App.ErrCounter++;
            CFE_EVS_SendEvent(ARANYA_EP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "FORWARD without SET_DEST");
            break;
        }

        if (ARANYA_EP_AuthorizeForward())
        {
            ARANYA_EP_ForwardToDest(cmd);
            ARANYA_EP_App.AuthorizedCount++;
            ARANYA_EP_App.CmdCounter++;
            CFE_EVS_SendEvent(ARANYA_EP_ROUTE_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "FORWARD routed to MID=0x%08lX FC=%u LEN=%u",
                              (unsigned long)CFE_SB_MsgIdToValue(ARANYA_EP_App.DestMsgId),
                              cmd->DestFcnCode, cmd->PayloadLen);
        }
        else
        {
            ARANYA_EP_App.DeniedCount++;
            CFE_EVS_SendEvent(ARANYA_EP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "FORWARD denied by Aranya");
        }
        break;
    }

    default:
        ARANYA_EP_App.ErrCounter++;
        CFE_EVS_SendEvent(ARANYA_EP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid CC=%u", (unsigned)FcnCode);
        break;
    }
}

/* Construct & send a destination SB command with given FC + payload */
void ARANYA_EP_ForwardToDest(const ARANYA_EP_ForwardCmd_t *cmd)
{
    uint8 rawbuf[CFE_MSG_MAX_MSG_SIZE]; /* generous stack buffer for test env */
    CFE_MSG_CommandHeader_t *hdr = (CFE_MSG_CommandHeader_t *)rawbuf;
    size_t size = CFE_MSG_CMD_HEADER_SIZE + cmd->PayloadLen;

    CFE_MSG_Init(hdr, ARANYA_EP_App.DestMsgId, size);
    CFE_MSG_SetFcnCode(hdr, cmd->DestFcnCode);

    if (cmd->PayloadLen > 0)
    {
        /* Copy payload bytes after header */
        memcpy(rawbuf + CFE_MSG_CMD_HEADER_SIZE, cmd->Payload, cmd->PayloadLen);
    }

    /* Transmit */
    CFE_SB_TransmitMsg((CFE_MSG_Message_t *)hdr, true);
}

/* Housekeeping */
void ARANYA_EP_SendHousekeeping(void)
{
    ARANYA_EP_HkTlm_t hk;
    memset(&hk, 0, sizeof(hk));
    CFE_MSG_Init(&hk.TlmHeader, CFE_SB_ValueToMsgId(ARANYA_EP_HK_TLM_MID), sizeof(hk));
    hk.CmdCounter      = ARANYA_EP_App.CmdCounter;
    hk.ErrCounter      = ARANYA_EP_App.ErrCounter;
    hk.AuthorizedCount = ARANYA_EP_App.AuthorizedCount;
    hk.DeniedCount     = ARANYA_EP_App.DeniedCount;
    hk.LastAuthResult  = ARANYA_EP_App.LastAuthResult;
    hk.DestMsgIdVal    = CFE_SB_MsgIdToValue(ARANYA_EP_App.DestMsgId);

    CFE_SB_TransmitMsg((CFE_MSG_Message_t *)&hk, true);
}
