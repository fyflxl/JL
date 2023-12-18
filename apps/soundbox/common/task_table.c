#include "system/includes.h"
#include "app_config.h"


/*任务列表, 注意:stack_size设置为32*n*/
const struct task_info task_info_table[] = {
    {"app_core",            1,     0,   896,  768  },
    {"sys_event",           6,     0,   256,   0    },
    {"btctrler",            4,     0,   512,   384  },
    {"btencry",             1,     0,   512,   128  },
#if TCFG_USER_TWS_ENABLE || TCFG_VIRTUAL_FAST_CONNECT_FOR_EMITTER
    {"tws",                 5,     0,   512,   128  },
#endif
#if (TCFG_USER_TWS_ENABLE || TCFG_USER_BLE_ENABLE || TCFG_USER_EMITTER_ENABLE)
    {"btstack",             3,     0,   768,   256  },
#else
    {"btstack",             3,     0,   512,   256  },
#endif
#if TCFG_DEC2TWS_TASK_ENABLE
    {"local_dec",           3,     1,   768,   128  },
#endif
#if ((TCFG_USER_TWS_ENABLE && TCFG_MIC_EFFECT_ENABLE)||(SOUNDCARD_ENABLE))
    {"audio_dec",           3,     1,   768 + 128 + 128,   128  },
#else
    {"audio_dec",           3,     1,   768 + 32,   128  },
#endif
#if ((defined TCFG_LIVE_AUDIO_LOW_LATENCY_EN) && TCFG_LIVE_AUDIO_LOW_LATENCY_EN)
    {"audio_enc",           5,     1,   512,   128  },
#else
    {"audio_enc",           3,     0,   512,   128  },
#endif
#if TCFG_AUDIO_DEC_OUT_TASK
    {"audio_out",           2,     0,   384,   0    },
#endif
    {"aec",					2,	   0,   768,   0	},
    {"aec_dbg",				3,	   0,   128,   128	},
    {"update",				1,	   0,   512,   0	},
#if(USER_UART_UPDATE_ENABLE)
    {"uart_update",	        1,	   0,   256,   128	},
#endif
    {"systimer",		    6,	   0,   128,   0    },
    {"dev_mg",           	3,     0,   512,   512  },
#ifndef CONFIG_SOUNDBOX_FLASH_256K
    {"usb_msd",           	1,     0,   512,   128  },
    {"usb_audio",           5,     0,   256,   256  },
    {"plnk_dbg",            5,     0,   256,   256  },
    {"adc_linein",          2,     0,   768,   128  },
    /* {"src_write",           1,     0,   768,   256 	}, */
    {"fm_task",             3,     0,   512,   128  },
#endif
    {"enc_write",           1,     0,   768,   0 	},
#if (RCSP_BTMATE_EN || RCSP_ADV_EN)
    {"rcsp_task",			4,	   0,   768,   128	},
#endif
    /* #if (TCFG_SPI_LCD_ENABLE||TCFG_SIMPLE_LCD_ENABLE) */
#if (TCFG_UI_ENABLE)
    {"ui",           	    2,      0,  1024,   1024  },
    /* {"ui",           	    2,     0,   768,   256  }, */
#else
    {"ui",                  3,     0,   384 - 64,   128  },
#endif

#if(TCFG_CHARGE_BOX_ENABLE)
    {"chgbox_n",            6,     0,   512,   128  },
#endif
#if (RCSP_MODE)
    {"rcsp",            4,     0,   768,   128  },
#endif//RCSP_MODE
#if (AI_APP_PROTOCOL)
    {"app_proto",           2,     0,   512,   64   },
    {"dw_update",           2,     0,   256,   128  },
#endif
#if RCSP_FILE_OPT
    {"file_bs",				1,	   0,   768,	  64  },
#endif
#if TCFG_KEY_TONE_EN
    {"key_tone",            5,     0,   256,   32},
#endif
#if TCFG_MIXER_CYCLIC_TASK_EN
    {"mix_out",             5,     0,   256,   0},
#endif

    {"mic_stream",          5,     0,   768,   128  },
#if(TCFG_HOST_AUDIO_ENABLE)
    {"uac_play",            6,     0,   768,   0    },
    {"uac_record",          6,     0,   768,   32   },
#endif
#if TCFG_MULTIPLE_MIC_REC_ENABLE
    {"data_export",         5,     0,   512,   256 },
#endif
#if TCFG_UNISOUND_ENABLE
    {"unisound",       		5,     1,   512,   256 },
#endif
#if TCFG_BROADCAST_ENABLE
    {"bcsync",              4,     0,   256,   64 },
#endif
    {0, 0},
};


