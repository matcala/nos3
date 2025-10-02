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
    case ARANYA_EP_EXP1_CC:
        if (ARANYA_EP_VerifyCmdLength(&ARANYA_EP_App.SbBufPtr->Msg, sizeof(ARANYA_EP_Exp1Cmd_t)) == CFE_SUCCESS)
        {
            // ARANYA_EP_Exp1Cmd_t *cmd = (ARANYA_EP_Exp1Cmd_t*)ARANYA_EP_App.SbBufPtr; // if needed
            ARANYA_EP_App.CmdCounter++;
            CFE_EVS_SendEvent(ARANYA_EP_EXP1_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "EXP1 command received");
        }
        break;
    case ARANYA_EP_EXP2_CC:
        if (ARANYA_EP_VerifyCmdLength(&ARANYA_EP_App.SbBufPtr->Msg, sizeof(ARANYA_EP_Exp2Cmd_t)) == CFE_SUCCESS)
        {
            // ARANYA_EP_Exp2Cmd_t *cmd = (ARANYA_EP_Exp2Cmd_t*)ARANYA_EP_App.SbBufPtr; // if needed
            ARANYA_EP_App.CmdCounter++;
            CFE_EVS_SendEvent(ARANYA_EP_EXP2_INF_EID, CFE_EVS_EventType_INFORMATION,
                              "EXP2 command received");
        }
        break;
    default:
        ARANYA_EP_App.ErrCounter++;
        CFE_EVS_SendEvent(ARANYA_EP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid CC=%u", (unsigned)FcnCode);
        break;
    }
}
