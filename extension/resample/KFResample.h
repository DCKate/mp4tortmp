#ifndef _RESAMPLING_AUDIO_H_
#define _RESAMPLING_AUDIO_H_

#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

typedef struct ResampleContext {
	struct SwrContext *swr_ctx;

	int src_rate;
	int dst_rate;
	int64_t src_ch_layout;
	int64_t dst_ch_layout;
	enum AVSampleFormat src_sample_fmt;
	enum AVSampleFormat dst_sample_fmt;

	int src_nb_channels;
	int dst_nb_channels;
	int src_linesize;
	int dst_linesize;
	int src_nb_samples;
	int dst_nb_samples;
	int max_dst_nb_samples;

	uint8_t **src_data;
	uint8_t **dst_data;
}ResampleContext;

ResampleContext* initAudioSwr(int samplerate,enum AVSampleFormat src_fmt,int src_samples,int64_t src_layout,int src_ch);
int do_audio_resample(char* outbuf,ResampleContext* g_rsmpl,char *pcm_data,int pcm_size);

//int resample(ResampleContext *rsmpl);
//int resample_init(ResampleContext *rsmpl);
int resample_deinit(ResampleContext *rsmpl);

#endif // _RESAMPLING_AUDIO_H_
