#ifndef _BROADCAST_DEC_H_
#define _BROADCAST_DEC_H_

#include "le_audio_stream.h"
#include "broadcast_codec.h"

extern const int JLA_2CH_L_OR_R;

void *broadcast_dec_open(struct broadcast_codec_params *params);
void broadcast_dec_close(void *priv);
void  broadcast_dec_kick_start(void *priv);

#endif
