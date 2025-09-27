#include "cfe.h"
#include "cfe_evs.h"
#include "cfe_es.h"
#include "cfe_sb.h"
#include "cfe_msg.h"
#include "osapi.h"

#include "aranya_ep_app.h"
#include "aranya_ep_utils.h"
#include <string.h>
#include <stdbool.h>

/* Global app data - defined here, declared extern in header */
ARANYA_EP_AppData_t ARANYA_EP_App;

/* Entry point */
void ARANYA_EP_AppMain(void)
{
    int32 status = CFE_SUCCESS;
    CFE_ES_PerfLogEntry(ARANYA_EP_PERF_ID);

    status = ARANYA_EP_Init();
    if (status != CFE_SUCCESS)
    {
        ARANYA_EP_App.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    // should be added to virtual fs now
    // (void)ARANYA_EP_ListDir("/test");

    while (CFE_ES_RunLoop(&ARANYA_EP_App.RunStatus))
    {
        CFE_ES_PerfLogExit(ARANYA_EP_PERF_ID);
        status = CFE_SB_ReceiveBuffer(&ARANYA_EP_App.SbBufPtr, ARANYA_EP_App.CmdPipeId, CFE_SB_PEND_FOREVER);
        CFE_ES_PerfLogEntry(ARANYA_EP_PERF_ID);

        if (status == CFE_SUCCESS)
        {
            ARANYA_EP_ProcessCommand();
        }
        else
        {
            CFE_EVS_SendEvent(ARANYA_EP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "ReceiveBuffer failed: 0x%08lX", (unsigned long)status);
        }
    }

    CFE_ES_PerfLogExit(ARANYA_EP_PERF_ID);
    CFE_ES_ExitApp(ARANYA_EP_App.RunStatus);
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
        if (ARANYA_EP_VerifyCmdLength(&ARANYA_EP_App.SbBufPtr->Msg, sizeof(ARANYA_EP_NoopCmd_t)) == CFE_SUCCESS)
        {
            ARANYA_EP_App.CmdCounter++;
            CFE_EVS_SendEvent(ARANYA_EP_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "NOOP v%u.%u.%u.%u",
                              ARANYA_EP_MAJOR_VERSION, ARANYA_EP_MINOR_VERSION,
                              ARANYA_EP_REVISION, ARANYA_EP_MISSION_REV);
        }
        break;
    case ARANYA_EP_RESET_CC:
        if (ARANYA_EP_VerifyCmdLength(&ARANYA_EP_App.SbBufPtr->Msg, sizeof(ARANYA_EP_ResetCmd_t)) == CFE_SUCCESS)
        {
            ARANYA_EP_App.CmdCounter = 0;
            ARANYA_EP_App.ErrCounter = 0;
            ARANYA_EP_App.AuthorizedCount = 0;
            ARANYA_EP_App.DeniedCount = 0;
            ARANYA_EP_App.LastAuthResult = ARANYA_EP_AUTH_UNKNOWN;
            CFE_EVS_SendEvent(ARANYA_EP_RESET_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "Counters reset");
        }
        break;
    case ARANYA_EP_SET_DEST_CC:
        if (ARANYA_EP_VerifyCmdLength(&ARANYA_EP_App.SbBufPtr->Msg, sizeof(ARANYA_EP_SetDestCmd_t)) == CFE_SUCCESS)
        {
            ARANYA_EP_SetDestCmd_t *cmd = (ARANYA_EP_SetDestCmd_t*)ARANYA_EP_App.SbBufPtr;
            ARANYA_EP_App.DestMsgId = CFE_SB_ValueToMsgId(cmd->DestMsgIdVal);
            ARANYA_EP_App.DestSet = true;
            ARANYA_EP_App.CmdCounter++;
            CFE_EVS_SendEvent(ARANYA_EP_SETDEST_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "Dest MID=0x%08lX", (unsigned long)cmd->DestMsgIdVal);
        }
        break;
    case ARANYA_EP_SET_UDS_CC:
        if (ARANYA_EP_VerifyCmdLength(&ARANYA_EP_App.SbBufPtr->Msg, sizeof(ARANYA_EP_SetUdsCmd_t)) == CFE_SUCCESS)
        {
            ARANYA_EP_SetUdsCmd_t *cmd = (ARANYA_EP_SetUdsCmd_t*)ARANYA_EP_App.SbBufPtr;
            if (ARANYA_EP_ValidateUdsPath(cmd->UdsPath))
            {
                strncpy(ARANYA_EP_App.UdsPath, cmd->UdsPath, sizeof(ARANYA_EP_App.UdsPath)-1);
                ARANYA_EP_App.UdsPath[sizeof(ARANYA_EP_App.UdsPath)-1] = '\0';
                CFE_EVS_SendEvent(ARANYA_EP_SOCKET_INF_EID, CFE_EVS_EventType_INFORMATION,
                                  "UDS path set (applies next init): %s", ARANYA_EP_App.UdsPath);
                ARANYA_EP_App.CmdCounter++;
            }
            else
            {
                ARANYA_EP_App.ErrCounter++;
                CFE_EVS_SendEvent(ARANYA_EP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                                  "Invalid UDS path");  
            }                                                                                                                                           
        }
        break;
    // case ARANYA_EP_FORWARD_CC:
    // {
    //     // Variable length; validate header portion first
    //     size_t full_len = 0;
    //     CFE_MSG_GetSize(&ARANYA_EP_App.SbBufPtr->Msg, &full_len);
    //     if (full_len < (sizeof(ARANYA_EP_ForwardCmd_t) - ARANYA_EP_MAX_FORWARD_LEN))
    //     {
    //         ARANYA_EP_App.ErrCounter++;
    //         CFE_EVS_SendEvent(ARANYA_EP_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
    //                           "FORWARD too short (%lu)", (unsigned long)full_len);
    //         break;
    //     }
    //     ARANYA_EP_ForwardCmd_t *cmd = (ARANYA_EP_ForwardCmd_t*)ARANYA_EP_App.SbBufPtr;
    //     size_t expected = (sizeof(ARANYA_EP_ForwardCmd_t) - ARANYA_EP_MAX_FORWARD_LEN + cmd->PayloadLen);
    //     if (cmd->PayloadLen > ARANYA_EP_MAX_FORWARD_LEN || full_len != expected)
    //     {
    //         ARANYA_EP_App.ErrCounter++;
    //         CFE_EVS_SendEvent(ARANYA_EP_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
    //                           "FORWARD len mismatch got=%lu exp=%lu payload=%u",
    //                           (unsigned long)full_len, (unsigned long)expected, cmd->PayloadLen);
    //         break;
    //     }
    //     if (!ARANYA_EP_App.DestSet)
    //     {
    //         ARANYA_EP_App.ErrCounter++;
    //         CFE_EVS_SendEvent(ARANYA_EP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
    //                           "FORWARD w/o destination");
    //         break;
    //     }
    //     if (ARANYA_EP_AuthorizeForward())
    //     {
    //         ARANYA_EP_ForwardToDest(cmd);
    //         ARANYA_EP_App.AuthorizedCount++;
    //         ARANYA_EP_App.CmdCounter++;
    //         CFE_EVS_SendEvent(ARANYA_EP_ROUTE_INF_EID, CFE_EVS_EventType_INFORMATION,
    //                           "FORWARD -> MID=0x%08lX FC=%u LEN=%u",
    //                           (unsigned long)CFE_SB_MsgIdToValue(ARANYA_EP_App.DestMsgId),
    //                           cmd->DestFcnCode, cmd->PayloadLen);
    //     }
    //     else
    //     {
    //         ARANYA_EP_App.DeniedCount++;
    //         CFE_EVS_SendEvent(ARANYA_EP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
    //                           "FORWARD denied");
    //     }
    //     break;
    // }
    default:
        ARANYA_EP_App.ErrCounter++;
        CFE_EVS_SendEvent(ARANYA_EP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid CC=%u", (unsigned)FcnCode);
        break;
    }
}
