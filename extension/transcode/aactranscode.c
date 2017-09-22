#include "aactranscode.h"

#define AAC_ENCODE_BITRATE 64000

int AACENC_Initial(AAC_ENC_t *aacenc, int channels, int sample_rate)
{
    memset(aacenc, 0, sizeof(AAC_ENC_t));
    TRANSPORT_TYPE adtsformat = TT_MP4_ADTS;
    aacenc->mode = channels;
    aacenc->aot = 5;    //AAC-LC = 2 , see FDK_audio.h, 5 = HE-AAC, 29 = HE-AAC v2.
    aacenc->eld_sbr = 0;
    aacenc->vbr = 0;
    aacenc->bitrate = AAC_ENCODE_BITRATE;
    aacenc->afterburner = 0;
    aacenc->samplerate = sample_rate;

	if (aacEncOpen(&aacenc->handle, 0, channels) != AACENC_OK) {
        printf("[%s:%d] Unable to open encoder\n", __FUNCTION__, __LINE__);
		return -1;
	}

	if (aacEncoder_SetParam(aacenc->handle, AACENC_AOT, aacenc->aot) != AACENC_OK) {
        printf("[%s:%d] Unable to set the AOT\n", __FUNCTION__, __LINE__);
        aacEncClose(&aacenc->handle);
		return -1;
	}

	if (aacenc->aot == 39 && aacenc->eld_sbr) {
		if (aacEncoder_SetParam(aacenc->handle, AACENC_SBR_MODE, 1) != AACENC_OK) {
            printf("[%s:%d] Unable to set SBR mode for ELD\n", __FUNCTION__, __LINE__);
            aacEncClose(&aacenc->handle);
			return -1;
		}
	}

	if (aacEncoder_SetParam(aacenc->handle, AACENC_SAMPLERATE, sample_rate) != AACENC_OK) {
        printf("[%s:%d] Unable to set the AOT\n", __FUNCTION__, __LINE__);
        aacEncClose(&aacenc->handle);
		return -1;
	}

	if (aacEncoder_SetParam(aacenc->handle, AACENC_CHANNELMODE, aacenc->mode) != AACENC_OK) {
        printf("[%s:%d] Unable to set the channel mode\n", __FUNCTION__, __LINE__);
        aacEncClose(&aacenc->handle);
		return -1;
	}

	if (aacEncoder_SetParam(aacenc->handle, AACENC_CHANNELORDER, 1) != AACENC_OK) {
        printf("[%s:%d] Unable to set the wav channel order\n", __FUNCTION__, __LINE__);
        aacEncClose(&aacenc->handle);
		return -1;
	}

	if (aacenc->vbr) {
		if (aacEncoder_SetParam(aacenc->handle, AACENC_BITRATEMODE, aacenc->vbr) != AACENC_OK) {
            printf("[%s:%d] Unable to set the VBR bitrate mode\n", __FUNCTION__, __LINE__);
            aacEncClose(&aacenc->handle);
			return -1;
		}
	} else {
		if (aacEncoder_SetParam(aacenc->handle, AACENC_BITRATE, aacenc->bitrate) != AACENC_OK) {
            printf("[%s:%d] Unable to set the bitrate\n", __FUNCTION__, __LINE__);
            aacEncClose(&aacenc->handle);
			return -1;
		}
	}

	if (aacEncoder_SetParam(aacenc->handle, AACENC_TRANSMUX, adtsformat) != AACENC_OK) {
        printf("[%s:%d] Unable to set the ADTS transmux\n", __FUNCTION__, __LINE__);
        aacEncClose(&aacenc->handle);
		return -1;
	}

	if (aacEncoder_SetParam(aacenc->handle, AACENC_AFTERBURNER, aacenc->afterburner) != AACENC_OK) {
        printf("[%s:%d] Unable to set the afterburner mode\n", __FUNCTION__, __LINE__);
        aacEncClose(&aacenc->handle);
		return -1;
	}

    /*
    if (aacEncoder_SetParam(aacenc->handle, AACENC_GRANULE_LENGTH, aacenc->info.frameLength) != AACENC_OK) {
        printf("[%s:%d] Unable to set the framelength mode\n", __FUNCTION__, __LINE__);
        aacEncClose(&aacenc->handle);
        return -1;
    }
    */

	if (aacEncEncode(aacenc->handle, NULL, NULL, NULL, NULL) != AACENC_OK) {
        printf("[%s:%d] Unable to initialize the encoder\n", __FUNCTION__, __LINE__);
        aacEncClose(&aacenc->handle);
		return -1;
	}

	if (aacEncInfo(aacenc->handle, &aacenc->info) != AACENC_OK) {
        printf("[%s:%d] Unable to get the encoder info\n", __FUNCTION__, __LINE__);
        aacEncClose(&aacenc->handle);
		return -1;
	}

#if 0
    printf("info.maxOutBufBytes = %d\n", aacenc->info.maxOutBufBytes);
    printf("info.maxAncBytes = %d\n", aacenc->info.maxAncBytes);
    printf("info.inBufFillLevel = %d\n", aacenc->info.inBufFillLevel);
    printf("info.inputChannels = %d\n", aacenc->info.inputChannels);
    printf("info.frameLength = %d\n", aacenc->info.frameLength);
    printf("info.encoderDelay = %d\n", aacenc->info.encoderDelay);
    printf("info.confSize = %d\n", aacenc->info.confSize);
#endif

    aacenc->encode_size = channels*2*aacenc->info.frameLength;
    aacenc->input_buf = (uint8_t*)malloc(aacenc->encode_size);
    if(aacenc->input_buf == NULL){
        printf("[%s:%d] Malloc input_buf error\n", __FUNCTION__, __LINE__);
        aacEncClose(&aacenc->handle);
		return -1;
    }

    aacenc->convert_buf = (int16_t*) malloc(aacenc->encode_size);
    if(aacenc->convert_buf == NULL){
        printf("[%s:%d] Malloc convert_buf error\n", __FUNCTION__, __LINE__);
        free(aacenc->input_buf);
        aacEncClose(&aacenc->handle);
        return -1;
    }

    aacenc->initial = 1;

    return 0;
}

int AACENC_DeInitial(AAC_ENC_t *aacenc)
{
    aacEncClose(&aacenc->handle);
    if(aacenc->input_buf != NULL){
        free(aacenc->input_buf);
        aacenc->input_buf = NULL;
    }
    if(aacenc->convert_buf != NULL){
        free(aacenc->convert_buf);
        aacenc->convert_buf = NULL;
    }
    memset(aacenc, 0 , sizeof(AAC_ENC_t));

    return 0;
}

int AACENC_Encode(AAC_ENC_t *aacenc, const uint8_t* inbuf, int inbufsize, uint8_t* outbuf, int outbufsize)
{
    int remainsize = 0;
    AACENC_BufDesc in_buf = { 0 }, out_buf = { 0 };
    AACENC_InArgs in_args = { 0 };
    AACENC_OutArgs out_args = { 0 };
    int in_identifier = IN_AUDIO_DATA;
    int in_size, in_elem_size;
    int out_identifier = OUT_BITSTREAM_DATA;
    int out_size, out_elem_size;
    int i;
    void *in_ptr, *out_ptr;
    AACENC_ERROR err;

    if(aacenc->input_buf == NULL){
        printf("[%s:%d] aacenc not initial\n", __FUNCTION__, __LINE__);
        return -1;
    }
    //Fill data into input_buf
    if((aacenc->input_buf_index + inbufsize) < aacenc->encode_size){
        //Save data into input_buf until encode_size
#if DEBUG_ON
        printf("[%s:%d] %s inbufsize: %d, encode_size: %d\n", __FILE__, __LINE__, __FUNCTION__, inbufsize, aacenc->encode_size);
#endif
        memcpy(aacenc->input_buf+aacenc->input_buf_index, inbuf, inbufsize);
        aacenc->input_buf_index += inbufsize;
        return 0;
    }
    else{
        remainsize = ((aacenc->input_buf_index + inbufsize) - aacenc->encode_size);
        memcpy(aacenc->input_buf+aacenc->input_buf_index, inbuf, inbufsize-remainsize);
        aacenc->input_buf_index = aacenc->encode_size;
    }


    //start encode
    for (i = 0; i < aacenc->encode_size/2; i++) {
        const uint8_t* in = &aacenc->input_buf[2*i];
        aacenc->convert_buf[i] = in[0] | (in[1] << 8);
    }
    in_ptr = aacenc->convert_buf;
    in_size = aacenc->encode_size;
    in_elem_size = 2;

    in_args.numInSamples = aacenc->encode_size/2;
    in_buf.numBufs = 1;
    in_buf.bufs = &in_ptr;
    in_buf.bufferIdentifiers = &in_identifier;
    in_buf.bufSizes = &in_size;
    in_buf.bufElSizes = &in_elem_size;

    out_ptr = outbuf;
    out_size = outbufsize;
    out_elem_size = 1;

    out_buf.numBufs = 1;
    out_buf.bufs = &out_ptr;
    out_buf.bufferIdentifiers = &out_identifier;
    out_buf.bufSizes = &out_size;
    out_buf.bufElSizes = &out_elem_size;

    err = aacEncEncode(aacenc->handle, &in_buf, &out_buf, &in_args, &out_args);

    if(err != AACENC_OK){
        printf("[%s:%d] Encoding failed\n", __FUNCTION__, __LINE__);
        return -1;
    }

    if (err == AACENC_ENCODE_EOF){
        printf("[%s:%d] Encoding AACENC_ENCODE_EOF\n", __FUNCTION__, __LINE__);
        return 0;
    }

    //denug("[%d] out_args.numOutBytes = %d\n", __LINE__, out_args.numOutBytes);

    memcpy(aacenc->input_buf, inbuf+(inbufsize-remainsize), remainsize);
    aacenc->input_buf_index = remainsize;
    return out_args.numOutBytes;
}
