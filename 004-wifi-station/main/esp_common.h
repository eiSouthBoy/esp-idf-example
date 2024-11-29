/*
 * @Description: 
 * @Author: 
 * @version: 
 * @Date: 
 * @LastEditors: 
 * @LastEditTime: 
 */

#ifndef __ESP_COMMON_H__
#define __ESP_COMMON_H__

//==============================================================================
// Include files
#include "esp_wifi_types.h"
#include "esp_chip_info.h"
#include "esp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

//==============================================================================
// Constants
#define MAX(a,b) ((a)>(b) ? (a):(b)) 
#define MIN(a,b) ((a)<(b) ? (a):(b))


//==============================================================================
// types


//==============================================================================
// global varibles



//==============================================================================
// global functions

/**
  * @brief  function description 
  *
  * @attention xxxx
  * 
  * @param[in]  input    xxxxx
  * 
  * @param[out]  output  xxxxx
  *
  * @return
  *    - OK: succeed
  *    - ERR: xxxxx
  */
extern void esp_wifi_authmode_get(wifi_auth_mode_t autumode_num, char authmode_name[34]);

extern void esp_chip_model_get(esp_chip_model_t model_num, char model_name[20]);

extern void esp_chip_reset_reason(esp_reset_reason_t reason_num, char reason_name[20]);


#ifdef __cplusplus
}
#endif

#endif