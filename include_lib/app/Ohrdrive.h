
/**************************************************************************//**
 * @brief Implementation ohrl driver
 * @version 0.1.24  for 25hz acc
 ******************************************************************************
 * @section License
 * <b>(C) Copyright 2016 OhrAccurate Labs, http://www.OhrAccurate.com</b>
 *******************************************************************************
 *
 * This file is licensed under the OhrAccurate License Agreement. See the file
 * "OhrAccurate_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/
 
#ifndef _OHRL_H_ 
#define _OHRL_H_ 

//#include <math.h>
//#include <stddef.h>
//#include <stdlib.h>
//#include <string.h>
//#include "stdint.h"
//#include <stdbool.h>
#include "types.h"

#define OHRL_ON		1
#define OHRL_OFF 	0
#define ENABLE_HANDUP_DETECTION	
#define OHRL_CURRENT_CHANGED		1
#define OHRL_CURRENT_NOTCHANGE	0

#define OHRL_ENCRYPT_CHECK_FAIL	1
#define OHRL_ENCRYPT_CHECK_PASS	0
#define OHRL_ENCRYPT_DEVICEID_ERROR 2

#define SAMPLERATE_PEDO   25


/* ohr output:hr data structure. */
typedef struct {
	uint8_t heartrate;		// heart rate,  30 to 220
	int8_t quality;				// -30 to 30, the bigger the beter 
	uint8_t onwrist;      //0 - on wrist, >0  - off wrist	
	int16_t ppg;          //ppg value  
	int16_t amb;          //amb value
	uint16_t ppg_maxmin;  //the change value of ppg within 1.5sec    
} type_OHRL_output;


extern type_OHRL_output  OHRL_data;			

//typedef struct {
//	uint8_t datalen;                                       
//  int8_t  bytes[3*8];                                                     
//}typ_accfifodata16hz;//16hz 2G

//extern typ_accfifodata16hz accfifodata16hz;

/**@brief data process at ohr data ready 
 *
 * @details    this function put data into FIFO of ohr raw data, and run led gain control.
 * @note       call this function at si1171 data ready event !
 * @param[in]  OHRfifo     fifo of 16hz ohr raw data,
               ACCfifo     fifo of 25hz acc raw data,   
 * @retval     OHRL_CURRENT_CHANGED		ohr current need to change, call OHRL_GetLEDcurrent() to get new current 
 * 						 OHRL_CURRENT_NOTCHANGE	ohr current not need to change                               
 */ 

uint8_t OHRL_IrqDatatProcess(typ_fifodata * OHRfifo,typ_fifodata * ACCfifo);

//uint8_t OHRL_SLIrqDatatProcess(type_OHRL_input  *raw_instack);

/**@brief get ohr current value.
 *
 * @details  
 * @note       
 * @retval     current value for OHR LED    
 */ 
uint8_t OHRL_GetLEDcurrent(void);

/**@brief     run ohr algorithm.
 *
 * @details   this function gets data from FIFO of ohr raw data, and run ohr algorithm, including pedometer function if it 
 *            is enabled, content of OHRL_data and PedoSummary is updated after ohr algorithm
 *            if no data in the FIFO, it returns immediately.
 * @note      call this function at main loop.  
 * @retval    OHRLLRUN_NODATA  there is no data in the FIFO which store the ohr raw data
 *            OHRLLRUN_HANDLED all data in the FIFO are handled, OHRL_data updated
 *            OHRLLRUN_NOENCRYPT	no encryption, algorithm is broken
 */
void  OHRL_algorithm(void);

/**@brief get string of ohrl version  
 *
 * @details   
 * @note                    
 * @retval    pointer of ohrl version  
							 the verion is a string AA.BB.CC.DD, where
							AA -  80~  means special verion for interface test
										00 to 10  normal release
							BB -  AFE version
										05 for si1181                        										
							CC -  verion of ohrl library
							DD -  verion of ohrl library	
 */
char *OHRL_GetOHRLVERSION(void);

/**@brief get string of ohr build version  
 *
 * @details   
 * @note                    
 * @retval    pointer of build version 
 							 the verion is a string YYYYMMDD.XX where
							YYYYMMDD -  year/month/date
							XX -  serial number at same day 
 */
char *OHRL_GetBUILDVERSION(void);


/**@brief ohr init 
 *
 * @details   do ohr initialize
 *
 * @note      call at ohr start 
 *             
 * @param[in]  block_id_encrypt     pointeer where to store encryption key
				       current               1~0x3F     startup current with auto gain control enabled 
																		 0            autocurrent	
						   max_current           0x30 to 0x3F  set max led current
							 config                BIT0   - 0   hrv is disabled, ppg sample rate is 16hz
							                                1   hrv is enabled, ppg sample rate is 32hz
																		 BIT1   - 0   lens type is CL831
																		          1   lens type is YL832
																		 BIT2:BIT3   - 0   HRV SMALL MOVEMENT ACCEPTABLE
																									 1
																									 2
																									 3   HRV MAXIMUM MOVEMENT ACCEPTABLE
																									 
							 ohrlogbuff            pointer to a buff to logdata, the buff length should must be 5x20+1
 * @retval    OHRL_ENCRYPT_CHECK_FAIL   encryption verify fail    
 *            OHRL_ENCRYPT_CHECK_PASS   encryption verify pass 
 */
//unsigned char OHRL_init(uint32_t* block_id_encrypt);
unsigned char OHRL_init(uint32_t* block_id_encrypt,uint8_t current,uint8_t max_current,uint8_t config, uint8_t *ohrlogbuff);


/*********************************************************************************************************************************
   OUTPUT of pedometer algorithm
*********************************************************************************************************************************/
typedef struct
{
  uint8_t instantsteps;  //immediate steps
	uint32_t TotalSteps_ul;  //total steps
	uint8_t  PedoSleep_flag;	// 0: active  1:Sleep
	uint8_t  PedoStatuschanged_flag;// 0: PedoSleep_flag not changed	1:PedoSleep_flag changed
																				// it is for monitor change of PedoSleep_flag.
  uint8_t   pace;                // pace in minute
	uint16_t  handdown_check_timer; //timer for display on at hand up , unit time is 1/SAMPLERATE_PEDO
	uint8_t   handup_Disp_flg;      //1 hand up, 2 hand down, app need reset to 0 once read it
	/* reference code for display on/off control by hand up detection
		if(PedoSummary.handup_Disp_flg==1) 
		{		
				PedoSummary.handdown_check_timer = SAMPLERATE_PEDO * 5; // max 5 seconds display on time
				PedoSummary.handup_Disp_flg = 0;
				display_on();
		}
		else if(PedoSummary.handup_Disp_flg==2)
		{
				PedoSummary.handup_Disp_flg = 0;
				display_off();
		}	
	*/
	
}type_PedoSummary; 
extern type_PedoSummary  PedoSummary;

/**@brief initialize step counter
 *
 * @details   initialize step counter, call at system startup
 *
 * @note      gsensor configure:  +-8G, 16bit, 25ODR
 *             
 * @param[in]  
							 dir_gsensor           set direction of x,y,z
							                       0:  z - point to watch face, x - point to finger
							                       1:  z - point to watch face, x - point to arm
																		 2:  z - point to watch face, y - point to finger
																		 3:  z - point to watch face, y - point to arm
																		 4:  z - point to watch bottom, x - point to finger
																		 5:  z - point to watch bottom, x - point to arm
																		 6:  z - point to watch bottom, y - point to finger
																		 7:  z - point to watch bottom, y - point to arm
								handup_enable          0: hand up detection disabled
																			 1: hand up detection enabled	
 */

void  Initial_Pedometer(uint8_t dir_gsensor, uint8_t handup_enable);

//push acc data for step alorithem at OHR is not enabled.
void  OHRL_pushXYZ_4G(typ_fifodata * fifo); //push data when acc sense range is 4G low power mode;
void  OHRL_pushXYZ_8G(typ_fifodata * fifo); //push data when acc sense range is 8G normal mode;

/**@brief get hrv rr.
 *
 * @details    get hrv rr, then clear existed rr data
 * @param[in]  adr  -- pointeer to store rr
 * @note       
 * @retval    total sets of rr 
 */ 
uint8_t  get_hrvRR(uint16_t *adr);

//logpedo = true - >log step data
void  OHRL_logStep(bool logpedo);

bool  OHRL_get_logStep(void);

uint8_t getRunstatus(void);
uint16_t get_Activity(void);
bool OHRL_get_onwristMovement(void);
uint8_t Get_OHR_mode(void);
uint8_t Get_ohr_ledoff(void);

#endif//_OHRL_H_
