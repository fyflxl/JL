
	OVERLAY : AT(0x200000) SUBALIGN(4)
    {
		.overlay_aec
		{
			aec_code_begin  = . ;
			*(.text._*)
			*(.data._*)
			*(.aec_code)
			*(.aec_const)
			*(.res_code)
			*(.res_const)
			*(.ns_code)
			*(.ns_const)
			*(.fft_code)
			*(.fft_const)
			aec_code_end = . ;
			aec_code_size = aec_code_end - aec_code_begin ;

			*(.msbc_enc)
			*(.cvsd_codec)
			*(.aec_bss)
			*(.aec_data)
			*(.res_data)
			*(.ns_data)
			*(.res_bss)
			*(.ns_bss)
			*(.aec_mem)
		}
		.overlay_aac
		{
			aac_dec_code_begin = .;
			*(.bt_aac_dec_code)
			*(.bt_aac_dec_sparse_code)
			aac_dec_code_end = .;
			aac_dec_code_size  = aac_dec_code_end - aac_dec_code_begin ;

			. = ALIGN(4);
			bt_aac_dec_const_begin = .;
			*(.bt_aac_dec_const)
			*(.bt_aac_dec_sparse_const)
			bt_aac_dec_const_end = .;
			bt_aac_dec_const_size = bt_aac_dec_const_end -  bt_aac_dec_const_begin ;

			*(.bt_aac_dec_data)
			*(.bt_aac_dec_bss)

			. = ALIGN(4);
			*(.aac_mem)
			*(.aac_ctrl_mem)
			/* 		. += 0x5fe8 ; */
			/*		. += 0xef88 ; */
		}
        /*
        .overlay_jla
        {
            jla_dec_code_begin = .;
            *(.jla_dec_code)
            jla_dec_code_end = .;
            jla_dec_code_size = jla_dec_code_end - jla_dec_code_begin;

            . = ALIGN(4);
            jla_dec_const_begin = .;
            *(.jla_dec_const)
            jla_dec_const_end = .;
            jla_dec_const_size = jla_dec_const_end - jla_dec_const_begin;
            *(.jla_dec_data)
            *(.jla_dec_bss)
        }
        */

		.overlay_mp3
		{
			*(.mp3_mem)
			*(.mp3_ctrl_mem)
			*(.mp3pick_mem)
			*(.mp3pick_ctrl_mem)
			*(.dec2tws_mem)
		}
		.overlay_wma
		{
			*(.wma_mem)
			*(.wma_ctrl_mem)
			*(.wmapick_mem)
			*(.wmapick_ctrl_mem)
		}
		.overlay_wav
		{
			*(.wav_mem)
			*(.wav_ctrl_mem)
		}
		.overlay_ape
        {
            *(.ape_mem)
            *(.ape_ctrl_mem)
		}
		.overlay_flac
        {
            *(.flac_mem)
            *(.flac_ctrl_mem)
		}
		.overlay_m4a
        {
            *(.m4a_mem)
            *(.m4a_ctrl_mem)
		}
		.overlay_amr
        {
            *(.amr_mem)
            *(.amr_ctrl_mem)
		}
		.overlay_dts
        {
            *(.dts_mem)
            *(.dts_ctrl_mem)
		}
		.overlay_fm
		{
			*(.fm_mem)
		}
        .overlay_pc
		{
#if (USB_MEM_USE_OVERLAY)
            *(.usb.data.bss.exchange)
#endif
		}
    } > ram0


