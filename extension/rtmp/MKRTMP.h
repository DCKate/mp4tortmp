#ifndef _MKRTMP_H_
#define _MKRTMP_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// for open h264 raw file.
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "srs_librtmp.h"

typedef struct rtmp_connection {
	srs_rtmp_t rtmp;
	// for multi rtmp streaming
	//srs_rtmp_t** rtmps;
//	pthread_t tid_rtmp;
//	pthread_mutex_t lock;
	u_int32_t ats;
	u_int32_t atd;
	int dts;
	int pts;
	char *pA;
	char *pV;
	int count;
	double fps;
}rtmp_connection;

int read_h264_frame(char* data, int size, char** pp, int* pnb_start_code, int fps, char** frame, int* frame_size, int* dts, int* pts);
int read_audio_frame(char* data, int size, char** pp, char** frame, int* frame_size);
int rtmp_write_video_frame(srs_rtmp_t rtmp, char *h264_raw, int file_size, char *p, double fps, int *dts, int *pts);
int rtmp_write_audio_frame(srs_rtmp_t rtmp, char *audio_aac, int aac_size, char *p, u_int32_t *timestamp, u_int32_t time_delta);
int rtmp_init(srs_rtmp_t rtmp);
int rtmp_deinit(srs_rtmp_t rtmp);

#endif // _MKRTMP_H_
