#include "app_config.h"

SECTIONS
{
    .text : ALIGN(4)
    {
        gsensor_dev_begin = .;
        KEEP(*(.gsensor_dev))
        gsensor_dev_end = .;

        fm_dev_begin = .;
        KEEP(*(.fm_dev))
        fm_dev_end = .;

        fm_emitter_dev_begin = .;
        KEEP(*(.fm_emitter_dev))
        fm_emitter_dev_end = .;

        storage_device_begin = .;
        KEEP(*(.storage_device))
        storage_device_end = .;

        wireless_dev_ops_begin = .;
        KEEP(*(.wireless_dev))
        wireless_dev_ops_end = .;

#if (!TCFG_CODE_RUN_RAM_FM_MODE)
 	    *(.usr_timer_const)
		*(.usr_timer_code)
    	*(.timer_const)
		*(.timer_code)
    	*(.cbuf_const)
		*(.cbuf_code)
		*(.fm_data_code)
		*(.fm_data_const)
		. = ALIGN(4);
#endif

#if  (!TCFG_LED7_RUN_RAM)
		. = ALIGN(4);
        *(.gpio_ram)
		. = ALIGN(4);
        *(.LED_code)
		. = ALIGN(4);
        *(.LED_const)
#endif

		. = ALIGN(4);
        *(.usb.text)
        *(.usb.text.const)

		. = ALIGN(32);
    } > code0

	.data ALIGN(32):
	{
		. = ALIGN(4);
        *(.usb.data)
        . = ALIGN(4);
	} > ram0

	.bss ALIGN(32):
    {
        *(.usb.data.bss.host_mem)  //由于usb有个bug，会导致dma写的数据超出预设的buf，最长可能写超1k，为了避免死机，所以usb dma buffer后面放一些其他模块的buff来避免死机
        *(.usb.data.bss)
#if (!USB_MEM_USE_OVERLAY)
        *(.usb.data.bss.exchange)
#endif
        . = ALIGN(32);
    } > ram0

	.data_code ALIGN(32):
	{
#if (TCFG_CODE_RUN_RAM_FM_MODE)
 	    *(.usr_timer_const)
		*(.usr_timer_code)
    	*(.timer_const)
		*(.timer_code)
    	*(.cbuf_const)
		*(.cbuf_code)
		*(.fm_data_code)
		*(.fm_data_const)
		. = ALIGN(4);
#endif

#if  (TCFG_LED7_RUN_RAM)
		. = ALIGN(4);
        *(.gpio_ram)
		. = ALIGN(4);
        *(.LED_code)
		. = ALIGN(4);
        *(.LED_const)
#endif
		. = ALIGN(4);
	} > ram0

}
