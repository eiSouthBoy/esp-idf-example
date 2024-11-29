//==============================================================================
// Include files
#include <string.h>
#include "esp_common.h"


//==============================================================================
// Constants


//==============================================================================
// types


//==============================================================================
// global varibles


//==============================================================================
// static varibles



//==============================================================================
// static functions



//==============================================================================
// global functions
void esp_wifi_authmode_get(wifi_auth_mode_t autumode_num, char authmode_name[34])
{
   switch (autumode_num)
   {
   case WIFI_AUTH_OPEN:
      strcpy(authmode_name, "WIFI_AUTH_OPEN");
      break;
   case WIFI_AUTH_WEP:
      strcpy(authmode_name, "WIFI_AUTH_WEP");
      break;
   case WIFI_AUTH_WPA_PSK:
      strcpy(authmode_name, "WIFI_AUTH_WPA_PSK");
      break;
   case WIFI_AUTH_WPA2_PSK:
      strcpy(authmode_name, "WIFI_AUTH_WPA2_PSK");
      break;
   case WIFI_AUTH_WPA_WPA2_PSK:
      strcpy(authmode_name, "WIFI_AUTH_WPA_WPA2_PSK");
      break;
   case WIFI_AUTH_ENTERPRISE:
      strcpy(authmode_name, "WIFI_AUTH_ENTERPRISE");
      break;
   case WIFI_AUTH_WPA3_PSK:
      strcpy(authmode_name, "WIFI_AUTH_WPA3_PSK");
      break;
   case WIFI_AUTH_WPA2_WPA3_PSK:
      strcpy(authmode_name, "WIFI_AUTH_WPA2_WPA3_PSK");
      break;
   case WIFI_AUTH_WAPI_PSK:
      strcpy(authmode_name, "WIFI_AUTH_WAPI_PSK");
      break;
   case WIFI_AUTH_OWE:
      strcpy(authmode_name, "WIFI_AUTH_OWE");
      break;
   case WIFI_AUTH_WPA3_ENT_192:
      strcpy(authmode_name, "WIFI_AUTH_WPA3_ENT_192");
      break;
   case WIFI_AUTH_WPA3_EXT_PSK:
      strcpy(authmode_name, "WIFI_AUTH_WPA3_EXT_PSK");
      break;
   case WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE:
      strcpy(authmode_name, "WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE");
      break;
   case WIFI_AUTH_MAX:
      strcpy(authmode_name, "WIFI_AUTH_MAX");
      break;
   default:
      strcpy(authmode_name, "unknown");
      break;
   }
}

void esp_chip_model_get(esp_chip_model_t model_num, char model_name[20])
{
    switch(model_num)
    {
    case CHIP_ESP32: //!< ESP32
        strcpy(model_name, "ESP32");
        break;
    case CHIP_ESP32S2: //!< ESP32-S2
        strcpy(model_name, "ESP32S2");
        break;
    case CHIP_ESP32S3: //!< ESP32-S3
        strcpy(model_name, "ESP32S3");
        break;
    case CHIP_ESP32C3: //!< ESP32-C3
        strcpy(model_name, "ESP32C3");
        break;
    case CHIP_ESP32C2: //!< ESP32-C2
        strcpy(model_name, "ESP32C2");
        break;
    case CHIP_ESP32C6: //!< ESP32-C6
        strcpy(model_name, "ESP32C6");
        break;
    case CHIP_ESP32H2: //!< ESP32-H2
        strcpy(model_name, "ESP32H2");
        break;
    case CHIP_ESP32P4: //!< ESP32-P4
        strcpy(model_name, "ESP32P4");
        break;
    case CHIP_POSIX_LINUX:
        strcpy(model_name, "POSIX_LINUX");
        break;
    default:
        strcpy(model_name, "unknown");
        break;
    }
}

void esp_chip_reset_reason(esp_reset_reason_t reason_num, char reason_name[20])
{
    switch(reason_num)
    {
    case ESP_RST_UNKNOWN:    //!< Reset reason can not be determined
        strcpy(reason_name, "ESP_RST_UNKNOWN");
        break;
    case ESP_RST_POWERON:    //!< Reset due to power-on event
        strcpy(reason_name, "ESP_RST_POWERON");
        break;
    case ESP_RST_EXT:        //!< Reset by external pin (not applicable for ESP32)
        strcpy(reason_name, "ESP_RST_EXT");
        break;
    case ESP_RST_SW:         //!< Software reset via esp_restart
        strcpy(reason_name, "ESP_RST_SW");
        break;
    case ESP_RST_PANIC:      //!< Software reset due to exception/panic
        strcpy(reason_name, "ESP_RST_PANIC");
        break;
    case ESP_RST_INT_WDT:    //!< Reset (software or hardware) due to interrupt watchdog
        strcpy(reason_name, "ESP_RST_INT_WDT");
        break;
    case ESP_RST_TASK_WDT:   //!< Reset due to task watchdog
        strcpy(reason_name, "ESP_RST_TASK_WDT");
        break;
    case ESP_RST_WDT:        //!< Reset due to other watchdogs
        strcpy(reason_name, "ESP_RST_WDT");
        break;
    case ESP_RST_DEEPSLEEP:  //!< Reset after exiting deep sleep mode
        strcpy(reason_name, "ESP_RST_DEEPSLEEP");
        break;
    case ESP_RST_BROWNOUT:   //!< Brownout reset (software or hardware)
        strcpy(reason_name, "ESP_RST_BROWNOUT");
        break;
    case ESP_RST_SDIO:       //!< Reset over SDIO
        strcpy(reason_name, "ESP_RST_SDIO");
        break;
    case ESP_RST_USB:        //!< Reset by USB peripheral
        strcpy(reason_name, "ESP_RST_USB");
        break;
    case ESP_RST_JTAG:       //!< Reset by JTAG
        strcpy(reason_name, "ESP_RST_JTAG");
       break;
    case ESP_RST_EFUSE:      //!< Reset due to efuse error
        strcpy(reason_name, "ESP_RST_EFUSE");
        break;
    case ESP_RST_PWR_GLITCH: //!< Reset due to power glitch detected
        strcpy(reason_name, "ESP_RST_PWR_GLITCH");
        break;
    case ESP_RST_CPU_LOCKUP: //!< Reset due to CPU lock up
        strcpy(reason_name, "ESP_RST_CPU_LOCKUP");
        break;
    default:
        strcpy(reason_name, "unknown");
        break;
    }
}





