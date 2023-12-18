#include "app_config.h"
#include "system/includes.h"

#include "asm/mcpwm.h"
#include "asm/clock.h"
#include "asm/gpio.h"
#include "qlohrmo2app.h"
#include "ohr_drv_afe.h"

///////////////////
/**
* @brief heart module intterupt event
*  
*/

#if 0
#if 0// HEART_SENSE_MODE_ENABELD

/**
* @brief sense事件方式
*  -此方法比GPIOTE方式稍微省电
*/
void ohr_int_senses_handler(void)
{
	BATTERY_INFO_T*batt = get_battery_info();
	if(batt->s_powerOffFlag == SLEEP_MODE)		
	{
		nrf_gpio_cfg_sense_input(OHR_INT, NRF_GPIO_PIN_PULLDOWN, NRF_GPIO_PIN_NOSENSE);
	}
	else if(nrf_gpio_pin_read(OHR_INT))
	{
		nrf_gpio_cfg_sense_input(OHR_INT, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
	}
	else
	{
		nrf_gpio_cfg_sense_input(OHR_INT, NRF_GPIO_PIN_PULLDOWN, NRF_GPIO_PIN_SENSE_HIGH);
		send_hr_message();
		hr_exeception_cnt = 0;
	}
	
}
#else
static uint8_t ohr_init_flag;
static void ohr_int_senses_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
	#if 0
	//BATTERY_INFO_T*batt = get_battery_info();
	//if(batt->s_powerOffFlag == SLEEP_MODE)		
      //return;

	if((pin == OHR_INT) && (nrf_gpio_pin_read(OHR_INT)==0))
	{
		send_hr_message();
		hr_exeception_cnt = 0;
	}
	#endif
	hr_exeception_cnt = 0;
}

#endif


/*****************OHR HANDLER***************************/

/**
* @brief 开启sense事件中断
*/
void enable_OHR_handler(void)
{
#if 0//HEART_SENSE_MODE_ENABELD
	 if(nrf_gpio_pin_read(OHR_INT))	
	{
		nrf_gpio_cfg_sense_input(OHR_INT, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_SENSE_LOW);
	}
	else
	{
		nrf_gpio_cfg_sense_input(OHR_INT, NRF_GPIO_PIN_PULLDOWN, NRF_GPIO_PIN_SENSE_HIGH);
	}
#else
	 if(ohr_init_flag == 0)
	 {
		 ohr_init_flag = 1;
    ret_code_t err_code;

    if (!nrf_drv_gpiote_is_init())
    {      
        err_code = nrf_drv_gpiote_init();    
        APP_ERROR_CHECK(err_code);
    }
    
    nrf_drv_gpiote_in_config_t in_config={          \
        .is_watcher = false,                        \
        .hi_accuracy = false,                     \
        .pull = NRF_GPIO_PIN_PULLUP,                \
        .sense = NRF_GPIOTE_POLARITY_TOGGLE,        \
    };
    
    err_code = nrf_drv_gpiote_in_init(OHR_INT, &in_config, ohr_int_senses_handler);
    APP_ERROR_CHECK(err_code);

    nrf_drv_gpiote_in_event_enable(OHR_INT, true);
	}
#endif
}

/**
* @brief 关闭sense事件中断
*/
void disable_OHR_handler(void)
{
#if HEART_SENSE_MODE_ENABELD
	nrf_gpio_cfg_sense_input(OHR_INT, NRF_GPIO_PIN_PULLUP, NRF_GPIO_PIN_NOSENSE);
	nrf_gpio_cfg_output(OHR_INT);
	nrf_gpio_pin_clear(OHR_INT); // 放电
#else
	if(ohr_init_flag)
	{
		  ohr_init_flag = 0;
			nrf_drv_gpiote_in_event_disable(OHR_INT);
			nrf_drv_gpiote_in_uninit(OHR_INT);
		  nrf_gpio_cfg_output(OHR_INT);
		  nrf_gpio_pin_clear(OHR_INT); // 放电
	  	log_info("2222222 disable_OHR_handler");
	}
	
	
#endif
	
}
#endif
/////////////////////
/*******************************  外部引脚中断参考代码  ***************************/

void (*qlio_isr_cbfun)(u8 index) = NULL;
void qlset_io_ext_interrupt_cbfun(void (*cbfun)(u8 index))
{
    qlio_isr_cbfun = cbfun;
}
___interrupt
void qlio_interrupt(void)
{
    u32 io_index = -1;
    if (JL_MCPWM->CH0_CON1 & BIT(15)) {
        JL_MCPWM->CH0_CON1 |= BIT(14);
        io_index = 0;
    } else if (JL_MCPWM->CH1_CON1 & BIT(15)) {
        JL_MCPWM->CH1_CON1 |= BIT(14);
        io_index = 1;
    } else if (JL_MCPWM->CH2_CON1 & BIT(15)) {
        JL_MCPWM->CH2_CON1 |= BIT(14);
        io_index = 2;
    } else if (JL_MCPWM->CH3_CON1 & BIT(15)) {
        JL_MCPWM->CH3_CON1 |= BIT(14);
        io_index = 3;
    } else {
        return;
    }
    if (qlio_isr_cbfun) {
        qlio_isr_cbfun(io_index);
    }
}
void qlio_ext_interrupt_cfg(u8 index, u8 port, u8 trigger_mode)
{
    if ((port > IO_PORT_MAX)||(index >=4)) {
        return;
    }

    gpio_set_die(port, 1);
    gpio_set_direction(port, 1);
    if (trigger_mode==1) {
        gpio_set_pull_up(port, 1);
        gpio_set_pull_down(port, 0);
        JL_MCPWM->FPIN_CON &= ~BIT(16 + index);//下降沿触发
    } else if (trigger_mode==0){
        gpio_set_pull_up(port, 0);
        gpio_set_pull_down(port, 1);
        JL_MCPWM->FPIN_CON |=  BIT(16 + index);//上升沿触发
    }
	else if (trigger_mode==2)
	{
		gpio_set_pull_up(port, 0);
        gpio_set_pull_down(port, 0);
        JL_MCPWM->FPIN_CON &= ~BIT(16 + index);//下降沿触发
	}
	else
	{
		gpio_set_pull_up(port, 0);
        gpio_set_pull_down(port, 0);
        JL_MCPWM->FPIN_CON |=  BIT(16 + index);//上升沿触发
	}
		
    JL_MCPWM->FPIN_CON |=  BIT(8 + index);//开启滤波
    JL_MCPWM->FPIN_CON |= (0b111111 << 0); //滤波时间 = 16 * 64 / hsb_clk (单位：s)

    gpio_ich_sel_iutput_signal(port, INPUT_CH_SIGNAL_MC_FPIN0 + index, INPUT_CH_TYPE_GP_ICH);
    request_irq(IRQ_MCPWM_CHX_IDX, 3, qlio_interrupt, 0);   //注册中断函数
    PWM_CH_REG *pwm_reg = get_pwm_ch_reg(index);
    pwm_reg->ch_con1 = BIT(14) | BIT(11) | BIT(4) | (index << 0);
    JL_MCPWM->MCPWM_CON0 |= BIT(index);
	
    printf("JL_MCPWM->CH%d_CON1  = 0x%x\n", index, pwm_reg->ch_con1);
    printf("JL_MCPWM->FPIN_CON   = 0x%x\n", JL_MCPWM->FPIN_CON);
    printf("JL_MCPWM->MCPWM_CON0 = 0x%x\n", JL_MCPWM->MCPWM_CON0);
	#if 0
    printf("JL_IOMAP->ICH_CON0   = 0x%x\n", JL_IOMAP->ICH_CON0);
    printf("JL_IOMAP->ICH_CON1   = 0x%x\n", JL_IOMAP->ICH_CON1);
    printf("JL_IOMAP->ICH_CON2   = 0x%x\n", JL_IOMAP->ICH_CON2);
    printf("JL_IOMAP->ICH_CON3   = 0x%x\n", JL_IOMAP->ICH_CON3);
    printf("JL_IMAP->FI_GP_ICH0  = %d\n", JL_IMAP->FI_GP_ICH0);
    printf("JL_IMAP->FI_GP_ICH1  = %d\n", JL_IMAP->FI_GP_ICH1);
    printf("JL_IMAP->FI_GP_ICH2  = %d\n", JL_IMAP->FI_GP_ICH2);
    printf("JL_IMAP->FI_GP_ICH3  = %d\n", JL_IMAP->FI_GP_ICH3);
    printf("JL_IMAP->FI_GP_ICH4  = %d\n", JL_IMAP->FI_GP_ICH4);
    printf("JL_IMAP->FI_GP_ICH5  = %d\n", JL_IMAP->FI_GP_ICH5);
    printf("JL_IMAP->FI_GP_ICH6  = %d\n", JL_IMAP->FI_GP_ICH6);
    printf("JL_IMAP->FI_GP_ICH7  = %d\n", JL_IMAP->FI_GP_ICH7);
    printf("JL_IMAP->FI_GP_ICH8  = %d\n", JL_IMAP->FI_GP_ICH8);
    printf("JL_IMAP->FI_GP_ICH9  = %d\n", JL_IMAP->FI_GP_ICH9);
	#endif
}
void qlio_ext_interrupt_close(u8 index, u8 port)
{
    if ((port > IO_PORT_MAX)||(index >=4)) {
        return;
    }
    gpio_set_die(port, 0);
    gpio_set_direction(port, 1);
    gpio_set_pull_up(port, 0);
    gpio_set_pull_down(port, 0);
    gpio_ich_disable_iutput_signal(port, INPUT_CH_SIGNAL_MC_FPIN0 + index, INPUT_CH_TYPE_GP_ICH);
    PWM_CH_REG *pwm_reg = get_pwm_ch_reg(index);
    pwm_reg->ch_con1 = BIT(14);
}

///////////// 使用举例如下 //////////////////
#if TCFG_OHRMO_EN

#define OHR_INT_CH 0
void ql_io_isr_cbfun(u8 index)
{
    printf("io -> %d\n", index);
	if(index ==OHR_INT_CH)
		cardiotachemoterInt();
}
void qlio_ext_interrupt_init(void)
{
    qlset_io_ext_interrupt_cbfun(ql_io_isr_cbfun);
	qlio_ext_interrupt_cfg(OHR_INT_CH, TCFG_IO_OHR_DVAIL, 0);//pc3
    //qlio_ext_interrupt_cfg(0, IO_PORTA_04, 1);
    //qlio_ext_interrupt_cfg(1, IO_PORTB_01, 1);
    //qlio_ext_interrupt_cfg(3, IO_PORTG_01, 1);
    //extern void wdt_clear();
    //while (1) {
    //    wdt_clear();
    //}
}
void qlio_ext_interrupt_unint(void)
{
	qlio_ext_interrupt_close(OHR_INT_CH, TCFG_IO_OHR_DVAIL);
}
#endif

