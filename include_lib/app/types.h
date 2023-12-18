

#ifndef TYPES_H__
#define TYPES_H__

#include "stdint.h"

//#define TRUE                           1
//#define FALSE                          0

//#if !defined(NULL)
//   #define NULL                        ((void *) 0)
//#endif


//#define SET  0x1
//#define CLEAR 0X0

//#define YES  0x1
//#define NO 0X0

//#define ON   0x1
//#define OFF 0X0

//#define SUCC 0x0
//#define FAIL 0Xff

#define HIGH_BYTE(word)           (uint8_t)((word >> 8) & 0x00FFu)                                                  /**< Get high byte of a uint16_t. */
#define LOW_BYTE(word)            (uint8_t)(word & 0x00FFu)                                                         /**< Get low byte of a uint16_t. */

//typedef void *            HANDLE;
//typedef char *            STRING;
//typedef int16_t               PT_RESULT;
//typedef int8_t                PT_BOOL;

typedef struct {
	uint8_t datalen;
    union                                                                     
    {                                                                         
        int16_t words[128];                                         
        uint8_t  bytes[128*2];                                                     
    }data;
}typ_fifodata;


//#define abs_us(a, b) ((a) < (b) ? (b-a) : (a-b))
	
#define BIT0 0x01 /**< The value of bit 0 */
#define BIT1 0x02 /**< The value of bit 1 */
#define BIT2 0x04 /**< The value of bit 2 */
#define BIT3 0x08 /**< The value of bit 3 */
#define BIT4 0x10 /**< The value of bit 4 */
#define BIT5 0x20 /**< The value of bit 5 */
#define BIT6 0x40 /**< The value of bit 6 */
#define BIT7 0x80 /**< The value of bit 7 */
#define BIT8 0x0100 /**< The value of bit 8 */
#define BIT9 0x0200 /**< The value of bit 9 */
#define BIT10 0x0400 /**< The value of bit 10 */
#define BIT11 0x0800 /**< The value of bit 11 */
#define BIT12 0x1000 /**< The value of bit 12 */
#define BIT13 0x2000 /**< The value of bit 13 */
#define BIT14 0x4000 /**< The value of bit 14 */
#define BIT15 0x8000 /**< The value of bit 15 */
#define BIT16 0x00010000 /**< The value of bit 16 */
#define BIT17 0x00020000 /**< The value of bit 17 */
#define BIT18 0x00040000 /**< The value of bit 18 */
#define BIT19 0x00080000 /**< The value of bit 19 */
#define BIT20 0x00100000 /**< The value of bit 20 */
#define BIT21 0x00200000 /**< The value of bit 21 */
#define BIT22 0x00400000 /**< The value of bit 22 */
#define BIT23 0x00800000 /**< The value of bit 23 */
#define BIT24 0x01000000 /**< The value of bit 24 */
#define BIT25 0x02000000 /**< The value of bit 25 */
#define BIT26 0x04000000 /**< The value of bit 26 */
#define BIT27 0x08000000 /**< The value of bit 27 */
#define BIT28 0x10000000 /**< The value of bit 28 */
#define BIT29 0x20000000 /**< The value of bit 29 */
#define BIT30 0x40000000 /**< The value of bit 30 */
#define BIT31 0x80000000 /**< The value of bit 31 */

#endif	//~_HRM_H


