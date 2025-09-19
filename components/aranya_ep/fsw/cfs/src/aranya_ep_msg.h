/*******************************************************************************
** File:
**   ARANYA_EP_msg.h
**
** Purpose:
**  Define ARANYA_EP application commands and telemetry messages
**
*******************************************************************************/
#ifndef _ARANYA_EP_MSG_H_
#define _ARANYA_EP_MSG_H_

// #include "cfe.h" // cfe-wide header that contains all headers
#include "cfe_msg.h"

/*
** Ground Command Codes
** TODO: Add additional commands required by the specific component
*/
#define ARANYA_EP_NOOP_CC          0
#define ARANYA_EP_RESET_CC         1
#define ARANYA_EP_SET_DEST_CC      2
#define ARANYA_EP_SET_UDS_CC       3
#define ARANYA_EP_FORWARD_CC       4

/*
** Telemetry Request Command Codes
** TODO: Add additional commands required by the specific component
*/
#define SAMPLE_REQ_HK_TLM   0
#define SAMPLE_REQ_DATA_TLM 1

/*
** Generic "no arguments" command type definition
*/
typedef struct
{
    /* Every command requires a header used to identify it */
    CFE_MSG_CommandHeader_t CmdHeader;

} ARANYA_EP_NoopCmd_t, ARANYA_EP_ResetCmd_t;

/* 
** Set destination (SB MsgId encoded as a 32-bit atom for portability) 
*/
typedef struct
{
    CFE_MSG_CommandHeader_t CmdHeader;
    uint32                  DestMsgIdVal;  /* use CFE_SB_ValueToMsgId at runtime */
} ARANYA_EP_SetDestCmd_t;

/* 
** Set UDS path (fixed-size string; null-terminated if shorter)
*/
typedef struct
{
    CFE_MSG_CommandHeader_t CmdHeader;
    char                    UdsPath[128];
} ARANYA_EP_SetUdsCmd_t;


/*
** SAMPLE housekeeping type definition
*/
typedef struct
{
    CFE_MSG_TelemetryHeader_t TlmHeader;
    uint8                     CmdCounter;
    uint8                     ErrCounter;
    uint32                    AuthorizedCount;
    uint32                    DeniedCount;
    uint32                    LastAuthResult;  /* 0=unknown, 1=authorized, 2=denied, 3=aranya_err */
    uint32                    DestMsgIdVal;    /* current destination MID value */

} __attribute__((packed)) ARANYA_EP_HkTlm_t;
#define ARANYA_EP_HK_LNGTH sizeof(ARANYA_EP_HkTlm_t)

#endif /* _ARANYA_EP_MSG_H_ */
