/************************************************************************
** File:
**    sample_events.h
**
** Purpose:
**  Define SAMPLE application event IDs
**
*************************************************************************/

#ifndef _ARANYA_EP_EVENTS_H_
#define _ARANYA_EP_EVENTS_H_

/*
Can be improved by grouping into CMD events and TLM events etc.
Look at SAMPLE_APP_EVENTS.H for reference
*/

/* Event ID assignments */
#define ARANYA_EP_INIT_INF_EID        1
#define ARANYA_EP_NOOP_INF_EID        2
#define ARANYA_EP_RESET_INF_EID       3
#define ARANYA_EP_CMD_ERR_EID        10
#define ARANYA_EP_LEN_ERR_EID        11
#define ARANYA_EP_ARANYA_ERR_EID     12
#define ARANYA_EP_ROUTE_INF_EID      13
#define ARANYA_EP_SETDEST_INF_EID    14
#define ARANYA_EP_SOCKET_INF_EID     15

/* DEBUG: Housekeeping telemetry successfully sent */
#define ARANYA_EP_HK_SENT_EID  0x00F0 

#endif /* _ARANYA_EP_EVENTS_H_ */
