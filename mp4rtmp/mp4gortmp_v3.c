#include <stdint.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h> 
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/channel_layout.h>
#include "../extension/transcode/aactranscode.h"
#include "../extension/rtmp/MKRTMP.h"
#include "../extension/resample/KFResample.h"
#include "../extension/vic/kqueue.h"
#include "../extension/vic/inc/vutil/vUtil.h"

#define KDLINE                      "%s@ %s : %s"
#define AUDIO_TMP_SIZE              20480   
#define ENABLE_AUDIO_FILE_DEBUG     0
#define AUDIO_FILE_AAC              "audio.aac"  // original file for AAC encoded
#define AUDIO_FILE_KKK              "audio.pcm"
#define SWRTRANSCODE                1
#define VIDEO_FRAME_RATE            30
#define VIDEO_FRAME                 1
#define AUDIO_FRAME                 2

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
static AVCodecContext *video_enc_ctx = NULL;
static AVCodec* pCodec = NULL;

static AVStream *video_stream = NULL, *audio_stream = NULL;
static const char *src_filename = NULL;

static int video_stream_idx = -1, audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket pkt;

static rtmp_connection crtmp;
static int initAudioPara = 0;
static int  initVideoEnc = 0;
static uint64_t lastVTs_UTC = 0;
static AAC_ENC_t *g_aacenc = NULL;

#if SWRTRANSCODE
static ResampleContext *g_rsmpl = NULL;
#endif

kqueue* videoQueue = NULL;
kqueue* audioQueue = NULL;
pthread_mutex_t lock;
pthread_t pid_vrtmp;
pthread_t pid_artmp;
int keep_live = 1;

void printColorHint(char* color,char* hint){
    // now
    time_t tt;
    struct tm *pt;
    time(&tt);
    pt = gmtime(&tt);
    char now_str[1024]={0};
    snprintf(now_str,1024,"[%d/%d/%d %d:%d:%d] --- ", (1900+pt->tm_year), (1+pt->tm_mon),pt->tm_mday,pt->tm_hour, pt->tm_min, pt->tm_sec);
    printf(KDLINE  CCe  "\n",color,now_str,hint);
}

uint64_t getSystimeTs ()
{

    struct timeval tp_now;
    gettimeofday(&tp_now,NULL);
    return tp_now.tv_sec*1000 + tp_now.tv_usec/1000 ; //<- uint64_t to avoid overrflow, rtmp timestamp is 24/32 bit
}

uint32_t setDefaultAudioTs(int samples,int samplerate){
    static uint32_t ats = 0;
    if (ats==0)
    {
        ats = getSystimeTs ();// use to get start, value is not important
    }else{
        ats += 1000*samples*2/samplerate;
    }
   
    return ats;
}

uint32_t setDefaultVideoTs(int samplerate){
    static uint32_t vts = 0;
    if (vts==0)
    {
        vts= getSystimeTs ();
    }else{
        vts += 1000/samplerate;
    }
    return vts;
}

uint64_t setDefaultTs(int samples,int samplerate){
    static int refcount = 1;
    static uint64_t vts = 0;
    static uint64_t ats = 0;
    uint32_t u = 0;

   
    if (samples>0){
        if(ats == 0){
            ats = getSystimeTs();
         }else{
            u = 1000*samples/(samplerate);
            ats+=u;
            if (vts !=0 && (vts-ats)>5*u){
                ats = vts+u;
            }
        }
        
    }else{
        if (vts==0){
            vts = getSystimeTs ();
        }else{
            u =(1000/samplerate);
            // usleep(u*1000);
            vts+=u;
            // if ((ats-vts-avdiff)>20*u){
            //     vts = ats+u;
            // } 
        }
       
    } 
    
    return (samples>0)?ats:vts;
}

void initAudio (int samplerate,int channel)
{
	if (initAudioPara)
	{
		return;
	}

	g_aacenc = (AAC_ENC_t *)malloc(sizeof(AAC_ENC_t));
 	memset(g_aacenc, 0, sizeof(AAC_ENC_t));

	if (g_aacenc->samplerate == 0) {
		AACENC_Initial(g_aacenc, channel, 44100);

	}
	initAudioPara = 1;

}
void destroyAudio ()
{
	if (!initAudioPara)
	{
		return;
	}
#if SWRTRANSCODE
	resample_deinit(g_rsmpl);
#endif
    AACENC_DeInitial(g_aacenc);
}

int audio_encodeAAC(char* aac_out,AAC_ENC_t *aacenc,char *audio_raw, int file_size){

   	int audio_aac_size = 0;

#if ENABLE_AUDIO_FILE_DEBUG
    int wret = 0;
    FILE *fd_aac = NULL;
    fd_aac = fopen(AUDIO_FILE_AAC, "a");
    if(fd_aac < 0){
        printf("open %s failed\n", AUDIO_FILE_AAC);
        fclose(fd_aac);
        return 0;
    }
#endif

   	if (file_size > 0) {
   		int remainsize = file_size;
   		int read_bufferindex = 0;
   		int bufferindex = 0;
   		while(remainsize>0){
   			char audio_aac_out[AUDIO_TMP_SIZE]={0};
   			char* tmp_rtmp_chunk = (char*)malloc(aacenc->encode_size);
   			int read_size = (remainsize>aacenc->encode_size)?aacenc->encode_size:remainsize;

   			memcpy(tmp_rtmp_chunk,audio_raw+read_bufferindex,read_size);
   			read_bufferindex+=read_size;
   			remainsize-=read_size;
   			audio_aac_size = AACENC_Encode(aacenc, (const uint8_t*)tmp_rtmp_chunk, read_size, (uint8_t *)audio_aac_out, AUDIO_TMP_SIZE); //726
   			free(tmp_rtmp_chunk);

	        if (audio_aac_size > 0) {
				memcpy(aac_out+bufferindex,audio_aac_out,audio_aac_size);
				bufferindex+=audio_aac_size;
			}

#if ENABLE_AUDIO_FILE_DEBUG
	        if(audio_aac_size > 0 && fd_aac > 0){
	            wret = fwrite(audio_aac_out, sizeof(uint8_t), audio_aac_size, fd_aac);
	            if(wret == 0)printf("[%d] write error!\n", __LINE__);
	        }
#endif
   		}
      
	}
   
#if ENABLE_AUDIO_FILE_DEBUG
   	fclose(fd_aac);
#endif

    return audio_aac_size;
}
int initVideoEncContex(int in_w,int in_h){
    if(initVideoEnc){
        return 0;
    }
    AVDictionary *opts = NULL;
    pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if(pCodec==NULL){
        fprintf(stderr, "Unsupport codec\n");
        return -1;
    }

    video_enc_ctx = avcodec_alloc_context3(pCodec);

   // video_enc_ctx->codec_id = AV_CODEC_ID_H264;  
    video_enc_ctx->codec_type = AVMEDIA_TYPE_VIDEO;  
    video_enc_ctx->pix_fmt = AV_PIX_FMT_YUV420P;  
    video_enc_ctx->width = in_w;    
    video_enc_ctx->height = in_h;  
    video_enc_ctx->bit_rate = 400000;    
    video_enc_ctx->gop_size=VIDEO_FRAME_RATE;  
  
    video_enc_ctx->time_base = (AVRational){1,VIDEO_FRAME_RATE};
    //video_enc_ctx->time_base.num = 1;    
    //video_enc_ctx->time_base.den = 25;    
  
    //H264  
    video_enc_ctx->me_range = 16;  
    video_enc_ctx->max_qdiff = 4;  
    video_enc_ctx->qcompress = 0.6;  
    video_enc_ctx->qmin = 10;  
    video_enc_ctx->qmax = 51;  
  
    //Optional Param  
    video_enc_ctx->max_b_frames=0;  
    
    /* Init the decoders, with or without reference counting */
    av_dict_set(&opts, "refcounted_frames", "0", 0);
    if (avcodec_open2(video_enc_ctx, pCodec,&opts) < 0) {
        fprintf(stderr, "Failed to open codec\n");
        return -1;
    }
    initVideoEnc = 1;
    return 0;

}

int RtmpSendVideoFrame (unsigned int ts,char* frame_ptr,int frame_size)
{
	int ret = 0;

	pthread_mutex_lock(&lock);
	{
	
		crtmp.pV = frame_ptr;
		crtmp.pts = 	ts;
		crtmp.dts = 	ts;
		ret = rtmp_write_video_frame(
				crtmp.rtmp,
				frame_ptr,
				frame_size,
				crtmp.pV,
				crtmp.fps,
				&crtmp.dts,
				&crtmp.pts);
		crtmp.count++;
        // printf("Send video %lu\n",crtmp.pts );
	}
	pthread_mutex_unlock(&lock);

	return ret;
}

int RtmpSendAudioFrame (unsigned int ts ,char* frame_ptr,int frame_size)
{
	int ret = 0;
	pthread_mutex_lock(&lock);
	{
		crtmp.pA = frame_ptr;
		crtmp.ats = 	ts;
		ret = rtmp_write_audio_frame(
					    crtmp.rtmp,
						frame_ptr/*frame_ptr*/,
						frame_size,
						crtmp.pA,
						&crtmp.ats,
						crtmp.atd);
		//DDr;
		// printf("Send Audio %lu\n",crtmp.ats );
	}
	pthread_mutex_unlock(&lock);

	return ret;
}

void* RtmpVideoRoutine (void* arg){
    
    //sleep(5);
    while(keep_live){
        knode* node = pop_kqueue_node(videoQueue);
        if (node)
        {
            uint64_t vt = setDefaultTs(0,VIDEO_FRAME_RATE);           
            RtmpSendVideoFrame (vt,node->data,node->len);
            char ts_str[39];
            snprintf(ts_str,39 , "video timestamp %lu", vt);
            printColorHint(CCy,ts_str);
                  
            Free_pop_node(node);
            usleep(200000);
        }else{
            usleep(500000);
        }
        
    }
    pthread_exit(NULL);
}

void* RtmpAudioRoutine (void* arg){
    
    //sleep(5);
    while(keep_live){
        knode* node = pop_kqueue_node(audioQueue);
        if (node)
        {
            uint64_t at = setDefaultTs(1024,44100);
            RtmpSendAudioFrame (at,node->data,node->len);
            char ts_str[39];
            snprintf(ts_str,39 , "audio timestamp %llu", at);
            printColorHint(CCg,ts_str);
                                
            Free_pop_node(node);
            usleep(100000);
        }else{
            usleep(500000);
        }
        
    }
    pthread_exit(NULL);
}

void RtmpCreateRoutine(){
    videoQueue = init_kqueue();
    audioQueue = init_kqueue();

    int ret = pthread_mutex_init(&lock, NULL);
    if (ret != 0) {
        printf("Mutex_init for Kalay Queue failed.\n");
        return NULL;
    }

    pthread_create (&pid_vrtmp, NULL, RtmpVideoRoutine, NULL);
    pthread_create (&pid_artmp, NULL, RtmpAudioRoutine, NULL);

}

void RtmpDestroyRoutine(){

    pthread_join(pid_vrtmp,NULL);
    pthread_join(pid_artmp,NULL);
    pthread_mutex_destroy(&lock);
    clear_kqueue(videoQueue);
    clear_kqueue(audioQueue);

}

int audioFLTP2S16P_hand(char* output, AVFrame *audioFrame,int goplanar){

    int16_t outputBuffer[AUDIO_TMP_SIZE] = {0};
    int in_samples = audioFrame->nb_samples;
    //int in_linesize = audioFrame->linesize[0];
    int i=0;
    int outsize=audioFrame->channels*in_samples*sizeof(int16_t);
    float* inputChannel0 = (float*)audioFrame->extended_data[0];
#if ENABLE_AUDIO_FILE_DEBUG
    int wret = 0;
    FILE *fd_kkk = NULL;
    fd_kkk = fopen(AUDIO_FILE_KKK, "a");
    if(fd_kkk < 0){
        printf("open %s failed\n", AUDIO_FILE_KKK);
        fclose(fd_kkk);
        return 0;
    }
#endif
    // Mono
    if (audioFrame->channels==1) {
        for (i=0 ; i<in_samples ; i++) {
            float sample = *inputChannel0++;
            if (sample<-1.0f) sample=-1.0f; else if (sample>1.0f) sample=1.0f;
            outputBuffer[i] = (int16_t) (sample * 32767.0f);
        }
    }
    //Stereo
    else {
        float* inputChannel1 = (float*)audioFrame->extended_data[1];
        for (i=0 ; i<in_samples ; i++) {
            if (goplanar)
            {
                outputBuffer[i] = (int16_t) ((*inputChannel0++) * 32767.0f);
                outputBuffer[in_samples+i] = (int16_t) ((*inputChannel1++) * 32767.0f); // planr
            }else{
                outputBuffer[i*2] = (int16_t) ((*inputChannel0++) * 32767.0f);
                outputBuffer[i*2+1] = (int16_t) ((*inputChannel1++) * 32767.0f); // interleave
            }
              
        }

    }
#if ENABLE_AUDIO_FILE_DEBUG
    if(outsize > 0 && fd_kkk > 0){
        wret = fwrite(outputBuffer, sizeof(uint8_t), outsize, fd_kkk);
        if(wret == 0)printf("[%d] write error!\n", __LINE__);
    }
    fclose(fd_kkk);
#endif

     memcpy(output,outputBuffer,outsize);
     return outsize;
}


static int process_packet(int *got_frame, int cached)
{
    int ret = 0;
    int decoded = pkt.size;
    
    *got_frame = 0;
    
    if (pkt.stream_index == video_stream_idx) {
        /* decode video frame */
       
        ret = avcodec_send_packet(video_dec_ctx, &pkt);
        // In particular, we don't expect AVERROR(EAGAIN), because we read all
        // decoded frames with avcodec_receive_frame() until done.
        if (ret < 0)
            return ret == AVERROR_EOF ? 0 : ret;
        //printf("decode pts [%d] size %d\n", pkt.pts,pkt.size);
        // int r = 0;
        while((ret = avcodec_receive_frame(video_dec_ctx, frame))>=0){
            // r++;
            AVPacket encpkt;
            int got_packet = 0;

            initVideoEncContex(frame->width,frame->height);
            
            av_init_packet(&encpkt);
            //ret = avcodec_encode_video2(video_enc_ctx, &encpkt, frame, &got_packet);
            
            //(1 / FPS) * sample rate * frame number
            ret = avcodec_send_frame(video_enc_ctx, frame);
            if (ret < 0){
                fprintf(stderr, "Error encode video frame (%s)\n", av_err2str(ret));
                break;
                //return ret;
            }
            // int c = 0;
            while((ret = avcodec_receive_packet(video_enc_ctx, &encpkt))>=0){
                // c++;
                //uint64_t vt = setDefaultTs(0,VIDEO_FRAME_RATE);
                //printf("video pts %lu!!\n",vt);                 
                //RtmpSendVideoFrame (vt,(encpkt.data), encpkt.size);
                push_kqueue(videoQueue, (encpkt.data),VIDEO_FRAME, encpkt.size);
                av_packet_unref(&encpkt);
            }
            // printf("encode packets %d\n",c);

        }
        // printf("decode frames %d\n",r);

    } else if (pkt.stream_index == audio_stream_idx) {
        /* decode audio frame */

        ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);
        //ret =  avcodec_send_packet(audio_dec_ctx,&pkt);
        if (ret < 0) {
            fprintf(stderr, "Error decoding audio frame (%s)\n", av_err2str(ret));
            return ret;
        }
        /* Some audio decoders decode only part of the packet, and have to be
         * called again with the remainder of the packet data.
         * Sample: fate-suite/lossless-audio/luckynight-partial.shn
         * Also, some decoders might over-read the packet. */
        decoded = FFMIN(ret, pkt.size);
        
        if (*got_frame) {
            
            initAudio (frame->sample_rate, 1);
            size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);
            
            int slen = 0;
            char output[AUDIO_TMP_SIZE]={0};
#if SWRTRANSCODE
            if(!g_rsmpl){
                g_rsmpl = initAudioSwr(frame->sample_rate,frame->format,frame->nb_samples,frame->channel_layout, audio_dec_ctx->channels);
            }
            slen = do_audio_resample(output,g_rsmpl,frame->extended_data[0], unpadded_linesize);
#else            
            slen = audioFLTP2S16P_hand(output, frame,1);

#endif     
            char audio_aac_out[AUDIO_TMP_SIZE]={0};
            
	    	int aac_size = audio_encodeAAC(
	    		audio_aac_out, g_aacenc,output,slen);
			if (aac_size > 0) {
               
                //uint64_t at = setDefaultTs(frame->nb_samples,frame->sample_rate);
                // printf("audio_frame nb_samples:%d pts:%lu size %d\n",
                //     frame->nb_samples,
                //     at,
                //     unpadded_linesize);
				//RtmpSendAudioFrame (at,audio_aac_out,aac_size);
                push_kqueue(audioQueue, audio_aac_out,AUDIO_FRAME,aac_size);
			}

            /* Write the raw audio data samples of the first plane. This works
             * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
             * most audio decoders output planar audio, which uses a separate
             * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
             * In other words, this code will write only the first audio channel
             * in these cases.
             * You should use libswresample or libavfilter to convert the frame
             * to packed data. */
            //fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
        }
    }
    
    /* If we use frame reference counting, we own the data and need
     * to de-reference it when we don't use it anymore */
    if (*got_frame){
        av_frame_unref(frame);
    }
    
    return decoded;
}

static int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    AVCodec *dec = NULL;
    AVDictionary *opts = NULL;
    
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), src_filename);
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];
        
        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
        
        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }
        
        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
        
        // old method for avcodec_parameters_to_context
        //if((ret = avcodec_copy_context(*dec_ctx, st->codec))<0){
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }
        
        /* Init the decoders, with or without reference counting */
        av_dict_set(&opts, "refcounted_frames", "0", 0);
        if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }
    
    return 0;
}

int main(int argc, char const *argv[])
{
	int ret = 0, got_frame;
    
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input_mp4_file> <rtmp streaming>\n"
                "\n", argv[0]);
        exit(1);
    }

    src_filename = argv[1];
    
    crtmp.rtmp = srs_rtmp_create(argv[2]);
	if (crtmp.rtmp)
	{
		srs_human_trace("rtmp_url=%s", argv[2]);
		if(rtmp_init(crtmp.rtmp)<0){
			rtmp_deinit(crtmp.rtmp);
			return -1;
		}
	}else{
		return -2;
	}
    crtmp.fps = 25;
    crtmp.count = 0;
    /* register all formats and codecs */
    av_register_all();
    
    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        exit(1);
    }
    
    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }
    
    if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
        video_stream = fmt_ctx->streams[video_stream_idx];  
    }
    
    if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
        audio_stream = fmt_ctx->streams[audio_stream_idx];
    }
    RtmpCreateRoutine();

     /* dump input information to stderr */
    av_dump_format(fmt_ctx, 0, src_filename, 0);
    
    if (!audio_stream && !video_stream) {
        fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
        ret = 1;
        goto end;
    }

 	frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
    /* initialize packet, set data to NULL, let the demuxer fill it */
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    
    /* read frames from the file */
    while (av_read_frame(fmt_ctx, &pkt) >= 0) {
        AVPacket orig_pkt = pkt;
        do {
            ret = process_packet(&got_frame, 0);
            if (ret < 0)
                break;
            pkt.data += ret;
            pkt.size -= ret;
        } while (pkt.size > 0);
        av_packet_unref(&orig_pkt);
    }
    keep_live = 0;
    /* flush cached frames */
    pkt.data = NULL;
    pkt.size = 0;

end:
    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
	av_frame_free(&frame);
    RtmpDestroyRoutine();
    //clear_kqueue(mediaQueue);
	rtmp_deinit(crtmp.rtmp);
	return 0;
}