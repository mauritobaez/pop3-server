#ifndef PEEP_UTILS_H
#define PEEP_UTILS_H

#include "command_utils.h"
#include "client_status.h"

#define MAX_COMMAND_LENGTH 264

//values for state_t
#define AUTHENTICATION 0x01
#define AUTHENTICATED 0x02

//values for command_type_t
#define C_A 0       //a
#define C_Q 1       //q
#define C_U_P 2     //u+
#define C_U_M 3     //u-
#define C_U_Q 4     //u?
#define C_C_E 5     //c=
#define C_C_Q 6     //c?
#define C_M_E 7     //m=
#define C_M_Q 8     //m?
#define C_T_E 9     //t=
#define C_T_Q 10    //t?
#define C_CU_Q 11    //cu?
#define C_HC_Q 12    //hc?
#define C_RB_Q 13    //rb?
#define C_RE_Q 14    //re?
#define C_XE_Q 15    //xe?
#define C_H_Q 16    //h?

typedef struct peep_client {
    state_t state;
    char input[MAX_COMMAND_LENGTH];
    command_t* pending_command;
} peep_client;

#endif