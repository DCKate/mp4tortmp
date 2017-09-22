/*
#define ENABLE_FILE_DEBUG 1

#if ENABLE_FILE_DEBUG
#define AUDIO_FILE_RAW "audio.raw"
#define AUDIO_FILE_DEC "audio.pcm"
#define AUDIO_FILE_AAC "audio.aac"
#define AUDIO_FILE_RMP "audio.rmp"
#define VIDEO_FILE_RAW "video.raw"
#endif
*/
#include "MKRTMP.h"
#define DEBUG_ON 0
//rtmp_connection g_conn;

int read_h264_frame(char* data, int size, char** pp, int* pnb_start_code, int fps,
	char** frame, int* frame_size, int* dts, int* pts)
{
	char* p = *pp;

	if (!srs_h264_startswith_annexb(p, size - (p - data), pnb_start_code)) {
		srs_human_trace("a. h264 raw data invalid.");
		return -1;
	}

	*frame = p;
	p += *pnb_start_code;

	for (;p < data + size; p++) {
		if (srs_h264_startswith_annexb(p, size - (p - data), NULL)) {
			break;
		}
	}

	*pp = p;
	*frame_size = p - *frame;
	if (*frame_size <= 0) {
		srs_human_trace("b. h264 raw data invalid.");
		return -1;
	}

	//*dts += 1000 / fps;
	//*pts = *dts;

	return 0;
}

int read_audio_frame(char* data, int size, char** pp, char** frame, int* frame_size)
{
	char* p = *pp;

	if (!srs_aac_is_adts(p, size - (p - data))) {
		srs_human_trace("aac adts raw data invalid.");
		return -1;
	}

	*frame = p;

	p += srs_aac_adts_frame_size(p, size - (p - data));

	*pp = p;
	*frame_size = p - *frame;
	if (*frame_size <= 0) {
		srs_human_trace("aac adts raw data invalid.");
		return -1;
	}

	return 0;
}

int rtmp_write_video_frame(srs_rtmp_t rtmp, char *h264_raw, int file_size, char *p, double fps, int *dts, int *pts) {

//	printf("rtmp: %p, h264: %p, filelen: %d, p: %p, fps: %f, dts: %d, pts: %d", &rtmp, h264_raw, file_size, p, fps, *dts, *pts);

	char* data = NULL;
	int size = 0;
	int nb_start_code = 0;
	
	if (read_h264_frame(h264_raw, (int)file_size, &p, &nb_start_code, fps, &data, &size, dts, pts) < 0) {
		srs_human_trace("read a frame from file buffer failed.");
		return -1;
	}
	
	int ret = srs_h264_write_raw_frames(rtmp, h264_raw, file_size, *dts, *pts);
	if (ret != 0) {
		if (srs_h264_is_dvbsp_error(ret)) {
#if DEBUG_ON
			srs_human_trace("ignore drop video error, code=%d", ret);
#endif
		} else if (srs_h264_is_duplicated_sps_error(ret)) {
#if DEBUG_ON
			srs_human_trace("ignore duplicated sps, code=%d", ret);
#endif
		} else if (srs_h264_is_duplicated_pps_error(ret)) {
#if DEBUG_ON
			srs_human_trace("ignore duplicated pps, code=%d", ret);
#endif
		} else {
			srs_human_trace("send h264 raw data failed. ret=%d", ret);
			return ret;
		}
		
	}

#if DEBUG_ON
    u_int8_t nut = (char)h264_raw[nb_start_code] & 0x1f;
	srs_human_trace("sent packet: type=%s, time=%d, size=%d, fps=%.2f, b[%d]=%#x(%s)",
		srs_human_flv_tag_type2string(SRS_RTMP_TYPE_VIDEO), *dts, file_size, fps, nb_start_code, (char)h264_raw[nb_start_code],
		(nut == 7? "SPS":(nut == 8? "PPS":(nut == 5? "I":(nut == 1? "P":(nut == 9? "AUD":(nut == 6? "SEI":"Unknown"))))))
	);
#else
	srs_human_flv_tag_type2string(SRS_RTMP_TYPE_VIDEO);
#endif

	return 0;
}
int rtmp_write_audio_frame(srs_rtmp_t rtmp, char *audio_aac, int aac_size, char *p, u_int32_t *timestamp, u_int32_t time_delta) {
	char* data = NULL;
	int size = 0;
	char sound_format = 10; // AAC
	char sound_rate = 2;	// 11K
	char sound_size = 1;	// 16-bit
	char sound_type = 0;	// Mono
	//*timestamp += time_delta;

	//printf("timedelay [%d]\n",time_delta );
	int ret = 0;

	if (read_audio_frame(audio_aac, aac_size, &p, &data, &size) < 0) {
		srs_human_trace("read a frame from file buffer failed.");
		return -1;
	}

	 if ((ret = srs_audio_write_raw_frame(rtmp,
		 sound_format, sound_rate, sound_size, sound_type,
		 audio_aac, aac_size, *timestamp)) != 0
	 ) {
		srs_human_trace("send audio raw data failed. ret=%d", ret);
		return ret;
	 }
#if	DEBUG_ON
	 srs_human_trace("sent packet: type=%s, time=%d, size=%d, codec=%d, rate=%d, sample=%d, channel=%d",
		 srs_human_flv_tag_type2string(SRS_RTMP_TYPE_AUDIO), *timestamp, aac_size, sound_format, sound_rate, sound_size,
		 sound_type);
#else
	srs_human_flv_tag_type2string(SRS_RTMP_TYPE_AUDIO);
#endif
	// @remark, when use encode device, it not need to sleep.
	//usleep(1000 * time_delta);
	return 0;
}

int rtmp_init(srs_rtmp_t rtmp)
{
	if (srs_rtmp_handshake(rtmp) != 0) {
		srs_human_trace("simple handshake failed.");
		//goto rtmp_destroy;
		return -1;
	}
	srs_human_trace("simple handshake success");

	if (srs_rtmp_connect_app(rtmp) != 0) {
		srs_human_trace("connect vhost/app failed.");
		//goto rtmp_destroy;
		return -1;

	}
	srs_human_trace("connect vhost/app success");

	if (srs_rtmp_publish_stream(rtmp) != 0) {
		srs_human_trace("publish stream failed.");
		//goto rtmp_destroy;
		return -1;

	}
	srs_human_trace("publish stream success");

//rtmp_destroy:
#if 0
	srs_rtmp_destroy(rtmp);
	close(raw_fd);
	free(h264_raw);
#endif

	return 0;
}

int rtmp_deinit(srs_rtmp_t rtmp)
{
	srs_rtmp_destroy(rtmp);
	return 0;
}
