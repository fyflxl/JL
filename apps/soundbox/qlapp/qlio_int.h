
#ifndef _QLIO_INT_H_
#define _QLIO_INT_H_
#include "system/includes.h"
#include "app_config.h"


#include "stdint.h"



//------------ MACRO DEFINED -----------------------------
#if 0
#define OHR_INT                  5		//DAVAIL


extern volatile uint32_t hr_exeception_cnt;

extern void enable_OHR_handler(void);
extern void disable_OHR_handler(void);
#if HEART_SENSE_MODE_ENABELD
void ohr_int_senses_handler(void);
#endif
#endif

extern void qlio_ext_interrupt_init(void);
extern void qlio_ext_interrupt_unint(void);

#endif




