#ifndef __IMD_RGB_H__
#define __IMD_RGB_H__


enum {
    IMD_RGB_ISR_PND			= 1,
    IMD_RGB_ISR_EMPTY		= 2,
    IMD_RGB_ISR_RESET_ALLOW	= 3,
};


#define IMD_RGB_PND_IE()	SFR(JL_IMD->IMDRGB_CON0, 29, 1, 1)
#define IMD_RGB_PND_DIS()	SFR(JL_IMD->IMDRGB_CON0, 29, 1, 0)


struct imd_rgb_config_def {	// RGB屏
    int dat_l2h;
    int dat_sf;
    int hv_mode;
    int xor_edge;
    int out_format;
    int continue_frames;

    int hsync_sel;
    int vsync_sel;
    int den_sel;

    int hpw_prd;
    int hbw_prd;
    int hfw_prd;
    int vpw_prd;
    int vbw_prd;
    int vfw_prd;

    void (*dat_dir)(u8);	// 数据方向
};



void imd_rgb_init(struct imd_rgb_config_def *priv);

void imd_rgb_draw(void);

int  imd_rgb_isr();



#endif

