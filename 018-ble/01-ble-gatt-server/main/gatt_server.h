#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Attributes State Machine */
enum
{
    IDX_SVC,
    IDX_CHAR_A,
    IDX_CHAR_VAL_A,
    IDX_CHAR_CFG_A,

    /* LED灯 */
    IDX_CHAR_LED,
    IDX_CHAR_VAL_LED,

    /* 温湿度传感器 */
    IDX_CHAR_TEMP,
    IDX_CHAR_VAL_TEMP,
    IDX_CHAR_CFG_TEMP,

    IDX_CHAR_B,
    IDX_CHAR_VAL_B,

    IDX_CHAR_C,
    IDX_CHAR_VAL_C,

    HRS_IDX_NB,
};