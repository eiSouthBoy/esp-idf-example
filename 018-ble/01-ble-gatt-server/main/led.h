/*
 * @Description: 
 * @Author: 
 * @version: 
 * @Date: 
 * @LastEditors: 
 * @LastEditTime: 
 */

#ifndef __LED_H__
#define __LED_H__

// Include files
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

// Constants
#define MAX(a,b) ((a)>(b) ? (a):(b)) 
#define MIN(a,b) ((a)<(b) ? (a):(b))

#define EXAMPLE_LED_GPIO CONFIG_LED_GPIO_NUM
#define LED_ON 1
#define LED_OFF 0

// types


// global varibles



// global functions



extern void led_init(void);
extern void led_set_level(uint32_t level);


#ifdef __cplusplus
}
#endif

#endif