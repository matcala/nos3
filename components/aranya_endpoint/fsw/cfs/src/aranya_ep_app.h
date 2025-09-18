/*******************************************************************************
** File: aranya_ep_app.h
**
** Purpose:
**   This is the main header file for the ARANYA_EP application.
**
*******************************************************************************/
#ifndef _ARANYA_EP_APP_H_
#define _ARANYA_EP_APP_H_

/*
** Include Files
*/
#include "cfe.h"
#include "aranya_ep_events.h"
#include "aranya_ep_msgids.h"
#include "aranya_ep_msg.h"
#include "aranya_ep_version.h"

/* Aranya C API */
#include "aranya-client.h"

/*
** Specified pipe depth - how many messages will be queued in the pipe
*/
#define ARANYA_EP_PIPE_DEPTH 16

/*
** UDS path buffer size
*/
#define ARANYA_EP_UDS_PATH_SIZE 128

/*
** Authorization result codes
*/
#define ARANYA_EP_AUTH_UNKNOWN   0
#define ARANYA_EP_AUTH_ALLOWED   1
#define ARANYA_EP_AUTH_DENIED    2
#define ARANYA_EP_AUTH_ERROR     3

/*
** ARANYA_EP global data structure
** The cFE convention is to put all global app data in a single struct.
** This struct is defined in the `aranya_ep_app.h` file with one global instance
** in the `.c` file.
*/
typedef struct
{
    /*
    ** Operational data - not reported in housekeeping
    */
    CFE_SB_PipeId_t   CmdPipeId;  /* Pipe Id for command pipe */
    CFE_SB_Buffer_t  *SbBufPtr;   /* Pointer to msg received on software bus */

    /*
    ** Housekeeping telemetry counters
    */
    uint8             CmdCounter;      /* Command counter */
    uint8             ErrCounter;      /* Error counter */
    uint32            AuthorizedCount; /* Count of authorized forwards */
    uint32            DeniedCount;     /* Count of denied forwards */
    uint32            LastAuthResult;  /* Last authorization result */

    /*
    ** Application configuration
    */
    CFE_SB_MsgId_t    DestMsgId;      /* Destination message ID for forwarding */
    bool              DestSet;        /* Flag indicating if destination is set */
    char              UdsPath[ARANYA_EP_UDS_PATH_SIZE]; /* Unix domain socket path */

    /*
    ** Aranya client state
    */
    struct AranyaClient ArClient;     /* Aranya client instance */
    bool                ArClientInit; /* Flag indicating if Aranya client is initialized */

} ARANYA_EP_AppData_t;

/*
** Exported Data
** Extern the global struct in the header for the Unit Test Framework (UTF).
*/
extern ARANYA_EP_AppData_t ARANYA_EP_App; /* ARANYA_EP App Data */

/*
**
** Local function prototypes.
**
** Note: Except for the entry point (ARANYA_EP_AppMain), these
**       functions are not called from any other source module.
*/
void  ARANYA_EP_AppMain(void);
int32 ARANYA_EP_Init(void);
void  ARANYA_EP_ProcessCommand(void);
void  ARANYA_EP_ProcessGroundCommand(CFE_MSG_FcnCode_t FcnCode);
void  ARANYA_EP_SendHousekeeping(void);
bool  ARANYA_EP_InitAranya(void);
bool  ARANYA_EP_AuthorizeForward(void);
void  ARANYA_EP_ForwardToDest(const ARANYA_EP_ForwardCmd_t *cmd);

#endif /* _ARANYA_EP_APP_H_ */
