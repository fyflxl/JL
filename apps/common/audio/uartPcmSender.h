#ifndef _UARTPCMSENDER_H_
#define _UARTPCMSENDER_H_
#include "system/includes.h"
#include "app_config.h"
/*
 *串口导出数据配置
 *注意IO口设置不要和普通log输出uart冲突
 */
#define PCM_UART1_TX_PORT			IO_PORTB_04//IO_PORT_DM	/*数据导出发送IO IO_PORTC_04//*/
#define PCM_UART1_RX_PORT			IO_PORTB_05//-1 //IO_PORTC_05//
#define PCM_UART1_BAUDRATE			9600//2000000		/*数据导出波特率,不用修改，和接收端设置一直*/

#if ((TCFG_UART0_ENABLE == ENABLE_THIS_MOUDLE) && (PCM_UART1_TX_PORT == TCFG_UART0_TX_PORT))
//IO口配置冲突，请检查修改
#error "PCM_UART1_TX_PORT conflict with TCFG_UART0_TX_PORT"
#endif/*PCM_UART1_TX_PORT*/




void uartSendInit();                        //串口发数初始化
void uartSendData(void *buf, u16 len); 		//发送数据的接口
void uartTickSend(void *priv);


#endif /*_UARTPCMSENDER_H_*/
