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
#include "aranya_ep_perfids.h"

/* Aranya C API */
#include "aranya-client.h"

#include <stdbool.h>
#include <string.h>

/*
** Specified pipe depth - how many messages will be queued in the pipe
*/
#define ARANYA_EP_PIPE_DEPTH 16

/*
** UDS path buffer size
*/
#define ARANYA_EP_UDS_PATH_SIZE 128

/*
** Default Daemon configs
*/
// #define ARANYA_EP_DEFAULT_DAEMON_PATH "/home/jstar/Desktop/daemon"
// #define ARANYA_EP_DEFAULT_DAEMON_MNT_PATH "/daemon"
#define ARANYA_EP_DEFAULT_UDS_PATH "/run/aranya/run/uds.sock"

/*
** Default AQC server address
*/
#define ARANYA_EP_DEFAULT_AQC_ADDR "127.0.0.1:11001"

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
    // TODO: change to bool?

    /*
    ** Application configuration
    */
    CFE_SB_MsgId_t    DestMsgId;      /* Destination message ID for forwarding */
    bool              DestSet;        /* Flag indicating if destination is set */
    char              UdsPath[ARANYA_EP_UDS_PATH_SIZE]; /* Unix domain socket path */

    /*
    ** Aranya client state
    */
    AranyaClient        Client;     /* Aranya client instance */
    bool                ClientInitialized; /* Flag indicating if Aranya client is initialized */

    /*
    ** Application run status
    */
    uint32 RunStatus; /* For CFE_ES_RunLoop (must be uint32 for CFE_ES_RunLoop) */

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
void  ARANYA_EP_ProcessCommand(void);
void  ARANYA_EP_ProcessGroundCommand(CFE_MSG_FcnCode_t FcnCode);

#endif /* _ARANYA_EP_APP_H_ */
