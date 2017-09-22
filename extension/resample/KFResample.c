#include "KFResample.h"

#define DEBUG_AUDIO_FILE    0 
#define DEBUG_ON            0
#define AUDIO_FILE_RMP      "audio.rmp"

int resample(ResampleContext *rsmpl) {

    int dst_len;
    int ret;

    /* compute destination number of samples */
    rsmpl->dst_nb_samples = av_rescale_rnd(swr_get_delay(rsmpl->swr_ctx, rsmpl->src_rate) +
                                    rsmpl->src_nb_samples, rsmpl->dst_rate, rsmpl->src_rate, AV_ROUND_UP);
    if (rsmpl->dst_nb_samples > rsmpl->max_dst_nb_samples) {
        av_free(rsmpl->dst_data[0]);
        ret = av_samples_alloc(rsmpl->dst_data, &rsmpl->dst_linesize, rsmpl->dst_nb_channels,
                               rsmpl->dst_nb_samples, rsmpl->dst_sample_fmt, 1);
        if (ret < 0)
            return -1;
        rsmpl->max_dst_nb_samples = rsmpl->dst_nb_samples;
    }
    /* convert to destination format */
    ret = swr_convert(rsmpl->swr_ctx, rsmpl->dst_data, rsmpl->dst_nb_samples, (const uint8_t **)rsmpl->src_data, rsmpl->src_nb_samples);
    if (ret < 0) {
        fprintf(stderr, "Error while converting\n");
        return -1;
    }
    dst_len = av_samples_get_buffer_size(&rsmpl->dst_linesize, rsmpl->dst_nb_channels,
                                             ret, rsmpl->dst_sample_fmt, 1);
#if DEBUG_ON
    printf("in:%d out:%d\n", rsmpl->src_nb_samples, ret);
#endif
    return dst_len;

}

int resample_deinit(ResampleContext *rsmpl) {
    if (rsmpl->src_data)
        av_freep(&rsmpl->src_data[0]);
    av_freep(&rsmpl->src_data);
    if (rsmpl->dst_data)
        av_freep(&rsmpl->dst_data[0]);
    av_freep(&rsmpl->dst_data);
    swr_free(&rsmpl->swr_ctx);
    return 0;
}

int resample_init(ResampleContext *rsmpl) {
    int ret;
    /* create resampler context */
    rsmpl->swr_ctx = swr_alloc();
    if (!rsmpl->swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        ret = AVERROR(ENOMEM);
        return -1;
    }
    /* set options */
    av_opt_set_int(rsmpl->swr_ctx, "in_channel_layout",     rsmpl->src_ch_layout, 0);
    av_opt_set_int(rsmpl->swr_ctx, "in_sample_rate",        rsmpl->src_rate, 0);
    av_opt_set_sample_fmt(rsmpl->swr_ctx, "in_sample_fmt",  rsmpl->src_sample_fmt, 0);
    av_opt_set_int(rsmpl->swr_ctx, "out_channel_layout",    rsmpl->dst_ch_layout, 0);
    av_opt_set_int(rsmpl->swr_ctx, "out_sample_rate",       rsmpl->dst_rate, 0);
    av_opt_set_sample_fmt(rsmpl->swr_ctx, "out_sample_fmt", rsmpl->dst_sample_fmt, 0);
    /* initialize the resampling context */
    if ((ret = swr_init(rsmpl->swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        return -1;
    }
    /* allocate source and destination samples buffers */
    rsmpl->src_nb_channels = av_get_channel_layout_nb_channels(rsmpl->src_ch_layout);
    ret = av_samples_alloc_array_and_samples(&rsmpl->src_data, &rsmpl->src_linesize, rsmpl->src_nb_channels,
                                             rsmpl->src_nb_samples, rsmpl->src_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate source samples\n");
        return -1;
    }
    /* compute the number of converted samples: buffering is avoided
     * ensuring that the output buffer will contain at least all the
     * converted input samples */
    rsmpl->max_dst_nb_samples = rsmpl->dst_nb_samples =
        av_rescale_rnd(rsmpl->src_nb_samples, rsmpl->dst_rate, rsmpl->src_rate, AV_ROUND_UP);
    /* buffer is going to be directly written to a rawaudio file, no alignment */
    rsmpl->dst_nb_channels = av_get_channel_layout_nb_channels(rsmpl->dst_ch_layout);
    ret = av_samples_alloc_array_and_samples(&rsmpl->dst_data, &rsmpl->dst_linesize, rsmpl->dst_nb_channels,
                                             rsmpl->dst_nb_samples, rsmpl->dst_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate destination samples\n");
        return -1;
    }

    return 0;

}

ResampleContext* initAudioSwr(int samplerate,enum AVSampleFormat src_fmt,int src_samples,int64_t src_layout,int src_ch){
    // if (swr_is_initialized(g_rsmpl->swr_ctx))
    // {
    //    return;
    // }
    printf("initialize Audio SWR\n");
    ResampleContext *g_rsmpl = (ResampleContext *) malloc(sizeof(ResampleContext));
    g_rsmpl->src_ch_layout = src_layout;
    g_rsmpl->dst_ch_layout = AV_CH_LAYOUT_MONO;// AV_CH_LAYOUT_STEREO;
    g_rsmpl->src_rate = samplerate;
    g_rsmpl->dst_rate = 44100;
    g_rsmpl->src_nb_channels = src_ch;
    g_rsmpl->dst_nb_channels = 1;
    g_rsmpl->src_nb_samples = src_samples;
    g_rsmpl->src_sample_fmt = src_fmt;
    g_rsmpl->dst_sample_fmt = AV_SAMPLE_FMT_S16P;
    resample_init(g_rsmpl);

    return g_rsmpl;

}
//char* do_audio_resample(int* out_size, char *pcm_data,int samples,int pcm_size)
int do_audio_resample(char* outbuf,ResampleContext* g_rsmpl,char *pcm_data,int pcm_size){
   
#if DEBUG_AUDIO_FILE
    int wret = 0;
    FILE *fd_rmp = NULL;
    fd_rmp = fopen(AUDIO_FILE_RMP, "a");
    if(fd_rmp < 0){
        printf("open %s failed\n", AUDIO_FILE_RMP);
        fclose(fd_rmp);
        return 0;
    }
#endif

    if (g_rsmpl) {
        memcpy(*g_rsmpl->src_data, pcm_data, pcm_size);
        pcm_size = resample(g_rsmpl);

        if (pcm_size > 0) {
            //audio_out = (char*) malloc(pcm_size*(int)sizeof(uint8_t));
            memcpy(outbuf,*g_rsmpl->dst_data, pcm_size*(int)sizeof(uint8_t));
            //memcpy(audio_out, *g_rsmpl->dst_data, pcm_size);

#if DEBUG_AUDIO_FILE
        if(fd_rmp > 0){
            wret = fwrite(outbuf, sizeof(uint8_t), pcm_size, fd_rmp);
            if(wret == 0)printf("[%d] write error!\n", __LINE__);
        }
        fclose(fd_rmp);
#endif

        }
    }
    
    //*out_size = pcm_size;
   
    return pcm_size*(int)sizeof(uint8_t);// rerurn audio_out;
}