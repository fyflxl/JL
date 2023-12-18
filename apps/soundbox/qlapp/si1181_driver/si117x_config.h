#ifndef SI117X_CONFIG
#define SI117X_CONFIG
//#include <stdint.h>
//#include <stdbool.h>
#include "system/includes.h"
#include "app_config.h"
#include "types.h"
#define OHRLIB_PRESENT   

#define SI117X_SAMPLERATE    16  
#define SI117X_SAMPLERATE_HRV    32  
#define READREQUENCY         4 // read times per second, SHOULD BE 4,2,1 

/* start of si117x, 
///input : current , led current at start up
//          config,  BIT0 - 0 16HZ  for hrv disabled
                            1  32HZ for hrv enabled
						         BIT1 - 0  yellow led is disabled
                        	  1  yellow led is enabled									 
// return  0 - inital success
//         <0  - error
*/
int16_t Si117x_Start(uint8_t current, uint8_t config);

/**************************************************************************//**
 * @brief  interrupt processing routine for Si117x, call at INT pin interrupt 
 *****************************************************************************/
void  HeartRateMonitor_interrupt_handle(typ_fifodata *acc_fifodata);

int16_t Si117x_checkstart(void);

void Si117x_Stop(void);
#endif
