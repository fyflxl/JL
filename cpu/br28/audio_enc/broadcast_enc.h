#ifndef _BROADCAST_ENC_H_
#define _BROADCAST_ENC_H_

#include "broadcast_codec.h"
#include "le_audio_stream.h"


void *broadcast_enc_open(struct broadcast_codec_params *params);
void broadcast_enc_close(void *priv);
void broadcast_enc_bitrate_updata(void *priv, u32 bit_rate);

#endif
