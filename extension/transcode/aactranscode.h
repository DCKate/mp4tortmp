#ifndef _AACTRANSCODE_H_
#define _AACTRANSCODE_H_

#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

//AAC Encoder
#include "aacenc_lib.h"

//libfdk-aac
typedef struct _AAC_ENC
{
    HANDLE_AACENCODER handle;
    CHANNEL_MODE mode;
    AACENC_InfoStruct info;
    int samplerate;
    int bitrate;
    int aot;
    int afterburner;
    int eld_sbr;
    int vbr;

    uint8_t *input_buf;
    int16_t* convert_buf;
    int input_buf_index;
    int encode_size;

    int initial;
}AAC_ENC_t;


int AACENC_Initial(AAC_ENC_t *aacenc, int channels, int sample_rate);
int AACENC_DeInitial(AAC_ENC_t *aacenc);
int AACENC_Encode(AAC_ENC_t *aacenc, const uint8_t* inbuf, int inbufsize, uint8_t* outbuf, int outbufsize);


#endif // _TRANSCODE_H_
