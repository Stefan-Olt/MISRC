/*
* simple_capture OS abstraction library
* Copyright (C) 2025 stefan_o
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SIMPLE_CAPTURE_H
#define SIMPLE_CAPTURE_H

#include <stdint.h>

#if defined(__linux__)
	#include <linux/videodev2.h>
	typedef uint32_t sc_codec_t;
	#define SC_CODEC_GREY  V4L2_PIX_FMT_GREY
	#define SC_CODEC_RGB24 V4L2_PIX_FMT_RGB24
	#define SC_CODEC_BGR24 V4L2_PIX_FMT_BGR24
	#define SC_CODEC_MJPEG V4L2_PIX_FMT_MJPEG
	#define SC_CODEC_YUYV  V4L2_PIX_FMT_YUYV
	#define SC_CODEC_UYVY  V4L2_PIX_FMT_UYVY
	#ifdef V4L2_PIX_FMT_YVYU
		#define SC_CODEC_YVYU V4L2_PIX_FMT_YVYU
	#endif
	#ifdef V4L2_PIX_FMT_VYUY
		#define SC_CODEC_VYUY V4L2_PIX_FMT_VYUY
	#endif
	#define SC_CODEC_EQUAL(a,b) (a==b)
	#define SC_CODEC_SET(a,b) (a=b)
#elif defined(__APPLE__)
#ifdef __APPLE__
	#include <CoreVideo/CoreVideo.h>
	#include <CoreMedia/CoreMedia.h>
	typedef FourCharCode sc_codec_t;
	#define SC_CODEC_GREY  kCVPixelFormatType_OneComponent8      /* 'L008' */
	#define SC_CODEC_RGB24 kCVPixelFormatType_24RGB              /* 24-bit RGB */
	#define SC_CODEC_BGR24 'BGR3'                                /* 24-bit BGR (not commonly output by AVF) */
	#define SC_CODEC_MJPEG 'MJPG'                                 /* rarely used in AVFoundation */
	#define SC_CODEC_YUYV  kCVPixelFormatType_422YpCbCr8_yuvs    /* 'yuvs' (YUY2 equivalent) */
	#define SC_CODEC_UYVY  kCVPixelFormatType_422YpCbCr8         /* '2vuy' */
	#define SC_CODEC_EQUAL(a,b) ((a)==(b))
	#define SC_CODEC_SET(a,b)   ((a)=(b))
	#endif
#elif defined(_WIN32)
	#include <windows.h>
	#include <mfapi.h>
	#include <mfobjects.h>
	#include <mfidl.h>
	#include <mfreadwrite.h>
	#include <mferror.h>
	#include <initguid.h>
	typedef GUID sc_codec_t;
	#ifndef MFVideoFormat_Y800
		/* Greyscale 8-bit (Y800). Not always defined in older headers */
		DEFINE_GUID(MFVideoFormat_Y800, 0x30303859, 0x0000, 0x0010, 0x80,0x00, 0x00,0xaa,0x00,0x38,0x9b,0x71); /* 'Y800' */
	#endif
	#ifndef MFVideoFormat_BGR3
		/* 24-bit BGR ('BGR3') */
		DEFINE_GUID(MFVideoFormat_BGR3, 0x33524742, 0x0000, 0x0010, 0x80,0x00, 0x00,0xaa,0x00,0x38,0x9b,0x71);
	#endif
	#ifndef MFVideoFormat_VYUY
		/* Some SDKs define this; if not, define here: 'VYUY' */
		DEFINE_GUID(MFVideoFormat_VYUY, 0x59555956, 0x0000, 0x0010, 0x80,0x00, 0x00,0xaa,0x00,0x38,0x9b,0x71);
	#endif
	#define SC_CODEC_GREY  MFVideoFormat_Y800
	#define SC_CODEC_RGB24 MFVideoFormat_RGB24
	#define SC_CODEC_BGR24 MFVideoFormat_BGR3
	#define SC_CODEC_MJPEG MFVideoFormat_MJPG
	#define SC_CODEC_YUYV  MFVideoFormat_YUY2
	#define SC_CODEC_UYVY  MFVideoFormat_UYVY
	#ifdef MFVideoFormat_YVYU
		#define SC_CODEC_YVYU  MFVideoFormat_YVYU
	#endif
	#ifdef MFVideoFormat_VYUY
		#define SC_CODEC_VYUY  MFVideoFormat_VYUY
	#endif
	#define SC_CODEC_EQUAL(a,b) IsEqualGUID(&(a), &(b))
	#define SC_CODEC_SET(a,b)   ((a)=(b))
#else
	#error "No implementation for this OS available"
#endif

typedef struct {
	char* name;       /* friendly name */
	char* device_id;  /* OS-specific identifier (path, GUID/symbolic link, uniqueID) */
} sc_capture_dev_t;

typedef struct {
	uint32_t num;   /* discrete num/den */
	uint32_t den;
	uint32_t min_num;  /* for range */
	uint32_t min_den;
	uint32_t max_num;
	uint32_t max_den;
	uint32_t step_num;
	uint32_t step_den;
} sc_fps_t;

typedef struct {
	uint32_t w;
	uint32_t h;
	sc_fps_t* fps;
	size_t n_fps;
} sc_size_t;

typedef struct {
	sc_codec_t codec;
	sc_size_t* sizes;
	size_t n_sizes;
} sc_formatlist_t;

typedef struct {
	void *ctx;
	void *data;
	uint32_t width;
	uint32_t height;
	uint32_t len;
	sc_codec_t codec;
} sc_data_info_t;

typedef struct sc_handle sc_handle_t;

typedef void(*sc_frame_callback_t)(sc_data_info_t *data_info);

char*	sc_get_impl_name();
char*	sc_get_impl_name_short();
size_t	sc_get_devices(sc_capture_dev_t **dev_list);
size_t	sc_get_formats(char* device_id, sc_formatlist_t **fmt_list);
int		sc_start_capture(const char* device_id, uint32_t width, uint32_t height, sc_codec_t codec,
			uint32_t fps_num, uint32_t fps_den, sc_frame_callback_t cb, void* cb_ctx, sc_handle_t** out_handle);
void	sc_stop_capture(sc_handle_t *handle);

#endif