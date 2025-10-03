#include "cfe_msg.h"

/* Complete Experiment CC */
// \camcmd CAM Experiment 1 - Small
#define CAM_EXP1_CC 10

/*
** CAM command message IDs
*/
#define CAM_CMD_MID     0x18C8
#define CAM_SEND_HK_MID 0x18C9

// Build and send a CAM capture command
typedef struct
{
    CFE_MSG_CommandHeader_t CmdHeader;

} CAM_NoArgsCmd_t;
#define CAM_NOARGSCMD_LNGTH sizeof(CAM_NoArgsCmd_t)