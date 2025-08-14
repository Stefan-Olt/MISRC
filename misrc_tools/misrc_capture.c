/*
* MISRC capture
* Copyright (C) 2024-2025  vrunk11, stefan_o
* 
* based on:
* hsdaoh - High Speed Data Acquisition over MS213x USB3 HDMI capture sticks
* Copyright (C) 2024 by Steve Markgraf <steve@steve-m.de>
* 
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

#if defined(__linux__)
#define _GNU_SOURCE
#include <sched.h>
#elif defined(__APPLE__) || defined(__MACH__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#if __STDC_VERSION__ >= 201112L && ! __STDC_NO_THREADS__ && ! _WIN32
#include <threads.h>
#else
#include "cthreads.h"
#warning "No C threads, fallback to pthreads/winthreads"
#endif
#include <time.h>

#ifndef _WIN32
	#include <endian.h>
	#include <getopt.h>
	#include <unistd.h>
	#define aligned_free(x) free(x)
	#define sleep_ms(x) usleep(x*1000)
#else
	#include <windows.h>
	#include <io.h>
	#include <fcntl.h>
	#if defined(__MINGW32__)
		#include <getopt.h>
	#else
		#include "getopt/getopt.h"
	#endif
	#define aligned_free(x) _aligned_free(x)
	#define aligned_alloc(a,s) _aligned_malloc(s,a)
	#define sleep_ms(x) Sleep(x)
	#define F_OK 0
	#define access _access
	#define le32toh(x) (x)
	#define le16toh(x) (x)
#endif

#include <hsdaoh.h>
#include <hsdaoh_raw.h>
#include <hsdaoh_crc.h>

#if LIBFLAC_ENABLED == 1
#include "FLAC/metadata.h"
#include "FLAC/stream_encoder.h"
# if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 14
static const char* const _FLAC_StreamEncoderSetNumThreadsStatusString[] = {
	"FLAC__STREAM_ENCODER_SET_NUM_THREADS_OK",
	"FLAC__STREAM_ENCODER_SET_NUM_THREADS_NOT_COMPILED_WITH_MULTITHREADING_ENABLED",
	"FLAC__STREAM_ENCODER_SET_NUM_THREADS_ALREADY_INITIALIZED",
	"FLAC__STREAM_ENCODER_SET_NUM_THREADS_TOO_MANY_THREADS"
};
# endif
#endif

#if LIBSWRESAMPLE_ENABLED == 1
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#endif

#include "ringbuffer.h"
#include "extract.h"
#include "wave.h"

#if LIBFLAC_ENABLED == 1 && defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 14
#include "numcores.h"
#endif

#define VERSION "0.4"
#define COPYRIGHT "licensed under GNU GPL v3 or later, (c) 2024-2025 vrunk11, stefan_o"

#define BUFFER_AUDIO_TOTAL_SIZE 65536*256
#define BUFFER_AUDIO_READ_SIZE 65536*3
#define BUFFER_TOTAL_SIZE 65536*1024
#define BUFFER_READ_SIZE 65536*32

#define _FILE_OFFSET_BITS 64

#define OPT_RESAMPLE_A     256
#define OPT_RESAMPLE_B     257
#define OPT_RF_FLAC_12BIT  258
#define OPT_AUDIO_4CH_OUT  259
#define OPT_AUDIO_2CH_12_OUT  260
#define OPT_AUDIO_2CH_34_OUT  261
#define OPT_AUDIO_1CH_1_OUT  262
#define OPT_AUDIO_1CH_2_OUT  263
#define OPT_AUDIO_1CH_3_OUT  264
#define OPT_AUDIO_1CH_4_OUT  265

#if defined(__GNUC__)
# define UNUSED(x) x __attribute__((unused))
#else
# define UNUSED(x) x
#endif

typedef struct {
	ringbuffer_t rb;
	ringbuffer_t rb_audio;
	int hsdaoh_frames_since_error;
	unsigned int hsdaoh_in_order_cnt;
	uint16_t hsdaoh_last_frame_cnt;
	uint16_t hsdaoh_last_crc[2];
	uint16_t hsdaoh_idle_cnt;
	bool hsdaoh_stream_synced;
	bool capture_rf;
	bool capture_audio;
	bool capture_audio_started;
	bool capture_audio_started2;
} capture_ctx_t;


typedef struct {
	ringbuffer_t rb;
	FILE *f;
#if LIBSWRESAMPLE_ENABLED == 1
	uint32_t resample_rate;
#endif
#if LIBFLAC_ENABLED == 1
	uint32_t flac_level;
	bool flac_verify;
	uint32_t flac_threads;
	uint8_t flac_bits;
#endif
} filewriter_ctx_t;

typedef struct {
	ringbuffer_t *rb;
	FILE *f_4ch;
	FILE *f_2ch[2];
	FILE *f_1ch[4];
	uint64_t total_bytes;
	bool non_4ch;
} audiowriter_ctx_t;


static int do_exit;
static int new_line = 1;
static hsdaoh_dev_t *dev = NULL;

static struct option getopt_long_options[] =
{
  {"device",               required_argument, 0, 'd'},
  {"count",                required_argument, 0, 'n'},
  {"time",                 required_argument, 0, 't'},
  {"overwrite",            no_argument,       0, 'w'},
  {"rf-adc-a",             required_argument, 0, 'a'},
  {"rf-adc-b",             required_argument, 0, 'b'},
  {"aux",                  required_argument, 0, 'x'},
  {"raw",                  required_argument, 0, 'r'},
  {"pad",                  no_argument,       0, 'p'},
  {"suppress-clip-rf-a",   no_argument,       0, 'A'},
  {"suppress-clip-rf-b",   no_argument,       0, 'B'},
#if LIBSWRESAMPLE_ENABLED == 1
  {"resample-rf-a",        required_argument, 0, OPT_RESAMPLE_A},
  {"resample-rf-b",        required_argument, 0, OPT_RESAMPLE_B},
#endif
#if LIBFLAC_ENABLED == 1
  {"rf-flac",              no_argument,       0, 'f'},
  {"rf-flac-12bit",        no_argument,       0, OPT_RF_FLAC_12BIT},
  {"rf-flac-level",        required_argument, 0, 'l'},
  {"rf-flac-verification", no_argument,       0, 'v'},
#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 14
  {"rf-flac-threads",      required_argument, 0, 'c'},
#endif
#endif
  {"audio-4ch",            required_argument, 0, OPT_AUDIO_4CH_OUT},
  {"audio-2ch-12",         required_argument, 0, OPT_AUDIO_2CH_12_OUT},
  {"audio-2ch-34",         required_argument, 0, OPT_AUDIO_2CH_34_OUT},
  {"audio-1ch-1",          required_argument, 0, OPT_AUDIO_1CH_1_OUT},
  {"audio-1ch-2",          required_argument, 0, OPT_AUDIO_1CH_2_OUT},
  {"audio-1ch-3",          required_argument, 0, OPT_AUDIO_1CH_3_OUT},
  {"audio-1ch-4",          required_argument, 0, OPT_AUDIO_1CH_4_OUT},
  {0, 0, 0, 0}
};

static char* yesno[] = {"no", "yes"};

static char* usage_options[][2] =
{
  { "device index (default: 0)", "[device index]" },
  { "number of samples to read (default: 0, infinite)", "[samples]" },
  { "time to capture (seconds, m:s or h:m:s; -n takes priority, assumes 40msps)", "[time]" },
  { "overwrite any files without asking", NULL },
  { "ADC A output file (use '-' to write on stdout)", "[filename]" },
  { "ADC B output file (use '-' to write on stdout)", "[filename]" },
  { "AUX output file (use '-' to write on stdout)", "[filename]" },
  { "raw data output file (use '-' to write on stdout)", "[filename]" },
  { "pad lower 4 bits of 16 bit output with 0 instead of upper 4]", NULL },
  { "suppress clipping messages for ADC A (need to specify -a or -r as well)", NULL },
  { "suppress clipping messages for ADC B (need to specify -b or -r as well)", NULL },
#if LIBSWRESAMPLE_ENABLED == 1
  { "resample ADC A signal to given sample rate (in kHz)", "[samplerate]" },
  { "resample ADC B signal to given sample rate (in kHz)", "[samplerate]" },
#endif
#if LIBFLAC_ENABLED == 1
  { "compress RF ADC output as FLAC", NULL },
  { "set RF FLAC sample width to 12 instead of 16 bit", NULL },
  { "set RF flac compression level (0-8, default: 1)", "[level]" },
  { "enable verification of RF flac encoder output", NULL },
#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 14
  { "number of RF flac encoding threads per file (default: auto)", "[threads]" },
#endif
#endif
  { "4 channel audio output (use '-' to write on stdout)", "[filename]" },
  { "stereo audio output of input 1/2 (use '-' to write on stdout)", "[filename]" },
  { "stereo audio output of input 3/4 (use '-' to write on stdout)", "[filename]" },
  { "mono audio output of input 1 (use '-' to write on stdout)", "[filename]" },
  { "mono audio output of input 2 (use '-' to write on stdout)", "[filename]" },
  { "mono audio output of input 3 (use '-' to write on stdout)", "[filename]" },
  { "mono audio output of input 4 (use '-' to write on stdout)", "[filename]" },
  {0, 0, 0, 0}
};

void create_getopt_string(char *getopt_string)
{
	char* s = getopt_string;
	int i = 0;
	while (1) {
		if (getopt_long_options[i].name == 0) break;
		if(getopt_long_options[i].val < 256) {
			*s = getopt_long_options[i].val;
			s++;
			if(getopt_long_options[i].has_arg != no_argument) {
				*s = ':';
				s++;
			}
		}
		i++;
	}
	*s = 0;
}

void usage(void)
{
	fprintf(stderr,
		"A simple program to capture from MISRC using hsdaoh\n\n"
		"Usage:\n"
	);
	int i = 0;
	while (1) {
		if (getopt_long_options[i].name == 0) break;
		if (getopt_long_options[i].val < 256) {
			fprintf(stderr," -%c, --%s", getopt_long_options[i].val, getopt_long_options[i].name);
		} else {
			fprintf(stderr," --%s", getopt_long_options[i].name);
		}
		if (getopt_long_options[i].has_arg == no_argument) {
			fprintf(stderr,":\n");
		}
		else {
			fprintf(stderr," %s:\n", usage_options[i][1]);
		}
		fprintf(stderr,"         %s\n", usage_options[i][0]);
		i++;
	}
	exit(1);
}

#ifdef _WIN32
BOOL WINAPI
sighandler(int signum)
{
	if (CTRL_C_EVENT == signum) {
		fprintf(stderr, "Signal caught, exiting!\n");
		do_exit = 1;
		hsdaoh_stop_stream(dev);
		return true;
	}
	return FALSE;
}
#else
static void sighandler(int UNUSED(signum))
{
	signal(SIGPIPE, SIG_IGN);
	fprintf(stderr, "Signal caught, exiting!\n");
	do_exit = 1;
	hsdaoh_stop_stream(dev);
}
#endif

static void print_capture_message(void UNUSED(*ctx), enum hsdaoh_msg_level UNUSED(level), const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	new_line = 1;
}

static void hsdaoh_callback(hsdaoh_data_info_t *data_info)
{
	capture_ctx_t *cap_ctx = data_info->ctx;
	metadata_t meta;
	uint32_t stream0_payload_bytes = 0;
	uint32_t stream1_payload_bytes = 0;
	int frame_errors = 0;
	uint8_t *buf_out;
	uint8_t *buf_out_audio;

	if (do_exit)
		return;

	if(cap_ctx) {
		hsdaoh_extract_metadata(data_info->buf, &meta, data_info->width);

		if (le32toh(meta.magic) != HSDAOH_MAGIC) {
			if (cap_ctx->hsdaoh_stream_synced) {
				print_capture_message(NULL,ERROR,"Lost sync to HDMI input stream\n");
			}
			cap_ctx->hsdaoh_stream_synced = false;
			return;
		}

		/* drop duplicated frames */
		if (meta.framecounter == cap_ctx->hsdaoh_last_frame_cnt)
			return;

		if (meta.framecounter != ((cap_ctx->hsdaoh_last_frame_cnt + 1) & 0xffff)) {
			cap_ctx->hsdaoh_in_order_cnt = 0;
			if (cap_ctx->hsdaoh_stream_synced)
				print_capture_message(NULL,ERROR,"Missed at least one frame, fcnt %d, expected %d!\n",
					meta.framecounter, ((cap_ctx->hsdaoh_last_frame_cnt + 1) & 0xffff));
		} else
			cap_ctx->hsdaoh_in_order_cnt++;

		cap_ctx->hsdaoh_last_frame_cnt = meta.framecounter;

		if (cap_ctx->capture_rf) while((buf_out = rb_write_ptr(&cap_ctx->rb, data_info->len))==NULL) {
			if (do_exit) return;
			print_capture_message(NULL,WARNING,"Cannot get space in ringbuffer for next frame (RF)\n");
			sleep_ms(4);
		}

		if (cap_ctx->capture_audio) while((buf_out_audio = rb_write_ptr(&cap_ctx->rb_audio, data_info->len))==NULL) {
			if (do_exit) return;
			print_capture_message(NULL,WARNING,"Cannot get space in ringbuffer for next frame (audio)\n");
			sleep_ms(4);
		}

		for (unsigned int i = 0; i < data_info->height; i++) {
			uint8_t *line_dat = data_info->buf + (data_info->width * sizeof(uint16_t) * i);

			/* extract number of payload words from reserved field at end of line */
			uint16_t payload_len = le16toh(((uint16_t *)line_dat)[data_info->width - 1]);
			uint16_t crc = le16toh(((uint16_t *)line_dat)[data_info->width - 2]);
			uint16_t stream_id = (meta.flags & FLAG_STREAM_ID_PRESENT) ? le16toh(((uint16_t *)line_dat)[data_info->width - 3]) : 0;

			/* we only use 12 bits, the upper 4 bits are reserved for the metadata */
			payload_len &= 0x0fff;

			if (payload_len > data_info->width-1) {
				if (cap_ctx->hsdaoh_stream_synced) {
					print_capture_message(NULL,ERROR,"Invalid payload length: %d\n", payload_len);
				}
				/* discard frame */
				return;
			}

			uint16_t idle_len = (data_info->width-1) - payload_len - ((meta.flags & FLAG_STREAM_ID_PRESENT) ? 1 : 0) - ((meta.crc_config == CRC_NONE) ? 0 : 1);
			frame_errors += hsdaoh_check_idle_cnt(dev, (uint16_t *)line_dat + payload_len, idle_len);

			if ((meta.crc_config == CRC16_1_LINE) || (meta.crc_config == CRC16_2_LINE)) {
				uint16_t expected_crc = (meta.crc_config == CRC16_1_LINE) ? cap_ctx->hsdaoh_last_crc[0] : cap_ctx->hsdaoh_last_crc[1];

				if ((crc != expected_crc) && cap_ctx->hsdaoh_stream_synced)
					frame_errors++;

				cap_ctx->hsdaoh_last_crc[1] = cap_ctx->hsdaoh_last_crc[0];
				cap_ctx->hsdaoh_last_crc[0] = crc16_ccitt(line_dat, data_info->width * sizeof(uint16_t));
			}

			if (payload_len > 0 && cap_ctx->hsdaoh_stream_synced) {
				if (cap_ctx->capture_rf && stream_id == 0 && (!cap_ctx->capture_audio || cap_ctx->capture_audio_started)) {
					memcpy(buf_out + stream0_payload_bytes, line_dat, payload_len * sizeof(uint16_t));
					//fprintf(stderr,"rf line, length: %i\n", payload_len * sizeof(uint16_t));
					stream0_payload_bytes += payload_len * sizeof(uint16_t);
				}
				else if (cap_ctx->capture_audio && stream_id == 1) {
					if(cap_ctx->capture_audio_started2) {
						memcpy(buf_out_audio + stream1_payload_bytes, line_dat, payload_len * sizeof(uint16_t));
						//fprintf(stderr,"audio line, length: %i\n", payload_len * sizeof(uint16_t));
						stream1_payload_bytes += payload_len * sizeof(uint16_t);
					}
					else {
						if (cap_ctx->capture_audio_started) {
							cap_ctx->capture_audio_started2 = true;
							print_capture_message(NULL,INFO,"Audio and RF now in sync\n");
						}
						else
							cap_ctx->capture_audio_started = true;
					}
				}
			}
		}

		if (frame_errors && cap_ctx->hsdaoh_stream_synced) {
			print_capture_message(NULL,ERROR,"%d frame errors, %d frames since last error\n", frame_errors, cap_ctx->hsdaoh_frames_since_error);
			cap_ctx->hsdaoh_frames_since_error = 0;
		} else {
			cap_ctx->hsdaoh_frames_since_error++;
			if (cap_ctx->capture_rf) rb_write_finished(&cap_ctx->rb, stream0_payload_bytes);
			if (cap_ctx->capture_audio) rb_write_finished(&cap_ctx->rb_audio, stream1_payload_bytes);
		}
		if (!cap_ctx->hsdaoh_stream_synced && !frame_errors && (cap_ctx->hsdaoh_in_order_cnt > 4)) {
			print_capture_message(NULL, INFO, "Syncronized to HDMI input stream\n MISRC uses CRC: %s\n MISRC uses stream ids: %s\n",
									yesno[((meta.crc_config == CRC_NONE) ? 0 : 1)], yesno[((meta.flags & FLAG_STREAM_ID_PRESENT) ? 1 : 0)]);
			if (cap_ctx->capture_audio) {
				if ((meta.flags & FLAG_STREAM_ID_PRESENT)) {
					print_capture_message(NULL,INFO,"Wait for RF and audio syncronisation...\n");
				}
				else {
					print_capture_message(NULL,CRITICAL,"MISRC does not transmit audio, cannot capture audio!\n");
					do_exit = 1;
					return;
				}
			}
			cap_ctx->hsdaoh_stream_synced = true;
		}
	}
}

int audio_file_writer(void *ctx)
{
	audiowriter_ctx_t *audio_ctx = ctx;
	size_t len = BUFFER_AUDIO_READ_SIZE;
	void *buf;
	wave_header_t h;
	bool convert_1ch = false;
	bool convert_2ch = false;
	uint8_t* buffer_1ch[4];
	uint8_t* buffer_2ch[2];
	memset(&h,0,sizeof(wave_header_t));
	audio_ctx->total_bytes = 0;
	if (audio_ctx->f_4ch != NULL && audio_ctx->f_4ch != stdout) fwrite(&h, 1, sizeof(wave_header_t), audio_ctx->f_4ch);
	for (int i=0; i<2; i++) {
		if (audio_ctx->f_2ch[i] != NULL) {
			if (audio_ctx->f_2ch[i] != stdout) fwrite(&h, 1, sizeof(wave_header_t), audio_ctx->f_2ch[i]);
			convert_2ch = true;
		}
	}
	for (int i=0; i<4; i++) {
		if (audio_ctx->f_1ch[i] != NULL) {
			if (audio_ctx->f_1ch[i] != stdout) fwrite(&h, 1, sizeof(wave_header_t), audio_ctx->f_1ch[i]);
			convert_1ch = true;
		}
	}
	if (convert_1ch) {
		if ((buffer_1ch[0] = aligned_alloc(32, BUFFER_AUDIO_READ_SIZE)) == NULL) {
			do_exit = 1;
			return -1;
		}
		for (int i=1; i<4; i++) buffer_1ch[i] = buffer_1ch[0] + (BUFFER_AUDIO_READ_SIZE/4)*i;
	}
	if (convert_2ch) {
		if ((buffer_2ch[0] = aligned_alloc(32, BUFFER_AUDIO_READ_SIZE)) == NULL) {
			do_exit = 1;
			return -1;
		}
		buffer_2ch[1] = buffer_2ch[0] + (BUFFER_AUDIO_READ_SIZE/2);
	}
	while(true) {
		while(((buf = rb_read_ptr(audio_ctx->rb, len)) == NULL) && !do_exit) {
			thrd_sleep(&(struct timespec){.tv_nsec=10000000}, NULL);
		}
		if (do_exit) {
			len = audio_ctx->rb->tail - audio_ctx->rb->head;
			if (len == 0) break;
			buf = rb_read_ptr(audio_ctx->rb, len);
		}
		if (audio_ctx->f_4ch != NULL) fwrite(buf, 1, len, audio_ctx->f_4ch);
		if (convert_1ch) extract_audio_1ch_C(buf, len, buffer_1ch[0], buffer_1ch[1], buffer_1ch[2], buffer_1ch[3]);
		if (convert_2ch) extract_audio_2ch_C(buf, len, (uint16_t*)buffer_2ch[0], (uint16_t*)buffer_2ch[1]);
		rb_read_finished(audio_ctx->rb, len);
		for (int i=0; i<2; i++) if (audio_ctx->f_2ch[i] != NULL) fwrite(buffer_2ch[i], 1, len/2, audio_ctx->f_2ch[i]);
		for (int i=0; i<4; i++) if (audio_ctx->f_1ch[i] != NULL) fwrite(buffer_1ch[i], 1, len/4, audio_ctx->f_1ch[i]);
		audio_ctx->total_bytes += len;
	}
	if (audio_ctx->f_4ch != NULL && audio_ctx->f_4ch != stdout) {
		fseek(audio_ctx->f_4ch, 0, SEEK_SET);
		create_wave_header(&h, audio_ctx->total_bytes/12, 78125, 4, 24);
		fwrite(&h, 1, sizeof(wave_header_t), audio_ctx->f_4ch);
		fclose(audio_ctx->f_4ch);
	}
	for (int i=0; i<2; i++) {
		if (audio_ctx->f_2ch[i] != NULL && audio_ctx->f_2ch[i] != stdout) {
			fseek(audio_ctx->f_2ch[i], 0, SEEK_SET);
			create_wave_header(&h, audio_ctx->total_bytes/12, 78125, 2, 24);
			fwrite(&h, 1, sizeof(wave_header_t), audio_ctx->f_2ch[i]);
			fclose(audio_ctx->f_2ch[i]);
		}
	}
	for (int i=0; i<4; i++) {
		if (audio_ctx->f_1ch[i] != NULL && audio_ctx->f_1ch[i] != stdout) {
			fseek(audio_ctx->f_1ch[i], 0, SEEK_SET);
			create_wave_header(&h, audio_ctx->total_bytes/12, 78125, 1, 24);
			fwrite(&h, 1, sizeof(wave_header_t), audio_ctx->f_1ch[i]);
			fclose(audio_ctx->f_1ch[i]);
		}
	}
	if (convert_1ch) aligned_free(buffer_1ch[0]);
	if (convert_2ch) aligned_free(buffer_2ch[0]);
	return 0;
}


int raw_file_writer(void *ctx)
{
	filewriter_ctx_t *file_ctx = ctx;
	size_t len = BUFFER_READ_SIZE;
	void *buf;
#if LIBSWRESAMPLE_ENABLED == 1
	/* setup resampling */
	struct SwrContext *swr_ctx;
	AVChannelLayout ch_layout;
	uint8_t *resample_buffer;
	int swr_ret;
	if (file_ctx->resample_rate!=0) {
		resample_buffer = aligned_alloc(32, BUFFER_READ_SIZE);
		if (!resample_buffer) {
			fprintf(stderr, "ERROR: failed allocating resampling buffer\n");
			do_exit = 1;
			return 0;
		}
		swr_ctx = swr_alloc();
		if (!swr_ctx) {
			fprintf(stderr, "ERROR: failed allocating resampling context\n");
			do_exit = 1;
			return 0;
		}
		ch_layout.nb_channels = 1;
		ch_layout.order = AV_CHANNEL_ORDER_UNSPEC;
		av_opt_set_chlayout(swr_ctx, "in_chlayout",      &ch_layout, 0);
		av_opt_set_int(swr_ctx, "in_sample_rate",        40000, 0);
		av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt",  AV_SAMPLE_FMT_S16, 0);
		av_opt_set_chlayout(swr_ctx, "out_chlayout",     &ch_layout, 0);
		av_opt_set_int(swr_ctx, "out_sample_rate",       file_ctx->resample_rate, 0);
		av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
		if (swr_init(swr_ctx) < 0) {
			fprintf(stderr, "ERROR: Failed to initialize the resampling context\n");
			do_exit = 1;
			return 0;
		}
	}
#endif
	while(true) {
		while(((buf = rb_read_ptr(&file_ctx->rb, len)) == NULL) && !do_exit) {
			//ms_sleep(10);
			thrd_sleep(&(struct timespec){.tv_nsec=10000000}, NULL);
		}
		if (do_exit) {
			len = file_ctx->rb.tail - file_ctx->rb.head;
			if (len == 0) break;
			buf = rb_read_ptr(&file_ctx->rb, len);
		}
#if LIBSWRESAMPLE_ENABLED == 1
		if (file_ctx->resample_rate!=0) {
			swr_ret = swr_convert(swr_ctx, (uint8_t **)&resample_buffer, len>>1, (const uint8_t **)&buf, len>>1);
			if (swr_ret < 0) {
				fprintf(stderr, "Error while converting\n");
				do_exit = 1;
				return 0;
			}
			fwrite(resample_buffer, 1, swr_ret<<1, file_ctx->f);
		} else {
			fwrite(buf, 1, len, file_ctx->f);
		}
#else
		fwrite(buf, 1, len, file_ctx->f);
#endif
		rb_read_finished(&file_ctx->rb, len);
	}
	if (file_ctx->f != stdout) fclose(file_ctx->f);
#if LIBSWRESAMPLE_ENABLED == 1
	if (file_ctx->resample_rate!=0) {
		aligned_free(resample_buffer);
		swr_free(&swr_ctx);
	}
#endif
	return 0;
}

#if LIBFLAC_ENABLED == 1
int flac_file_writer(void *ctx)
{
	filewriter_ctx_t *file_ctx = ctx;
	size_t len = BUFFER_READ_SIZE;
	void *buf;
	uint32_t ret;
	uint32_t srate = 40000;
	FLAC__bool ok = true;
	FLAC__StreamEncoder *encoder = NULL;
	FLAC__StreamEncoderInitStatus init_status;
	FLAC__StreamMetadata *seektable;
#if LIBSWRESAMPLE_ENABLED == 1
	/* setup resampling */
	struct SwrContext *swr_ctx;
	AVChannelLayout ch_layout;
	uint8_t *resample_buffer;
	int swr_ret;
	if (file_ctx->resample_rate!=0) {
		resample_buffer = aligned_alloc(32, BUFFER_READ_SIZE);
		if (!resample_buffer) {
			fprintf(stderr, "ERROR: failed allocating resampling buffer\n");
			do_exit = 1;
			return 0;
		}
		srate = file_ctx->resample_rate;
		swr_ctx = swr_alloc();
		if (!swr_ctx) {
			fprintf(stderr, "ERROR: failed allocating resampling context\n");
			do_exit = 1;
			return 0;
		}
		ch_layout.nb_channels = 1;
		ch_layout.order = AV_CHANNEL_ORDER_UNSPEC;
		av_opt_set_chlayout(swr_ctx, "in_chlayout",      &ch_layout, 0);
		av_opt_set_int(swr_ctx, "in_sample_rate",        40000, 0);
		av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt",  AV_SAMPLE_FMT_S32, 0);
		av_opt_set_chlayout(swr_ctx, "out_chlayout",     &ch_layout, 0);
		av_opt_set_int(swr_ctx, "out_sample_rate",       file_ctx->resample_rate, 0);
		av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S32, 0);
		if (swr_init(swr_ctx) < 0) {
			fprintf(stderr, "ERROR: Failed to initialize the resampling context\n");
			do_exit = 1;
			return 0;
		}
	}
#endif

	if((encoder = FLAC__stream_encoder_new()) == NULL) {
		fprintf(stderr, "ERROR: failed allocating FLAC encoder\n");
		do_exit = 1;
		return 0;
	}

	ok &= FLAC__stream_encoder_set_verify(encoder, file_ctx->flac_verify);
	ok &= FLAC__stream_encoder_set_compression_level(encoder, file_ctx->flac_level);
	ok &= FLAC__stream_encoder_set_channels(encoder, 1);
	ok &= FLAC__stream_encoder_set_bits_per_sample(encoder, file_ctx->flac_bits);
	ok &= FLAC__stream_encoder_set_sample_rate(encoder, srate);
	ok &= FLAC__stream_encoder_set_total_samples_estimate(encoder, 0);

	if(!ok) {
		fprintf(stderr, "ERROR: failed initializing FLAC encoder\n");
		do_exit = 1;
		return 0;
	}
#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 14
	ret = FLAC__stream_encoder_set_num_threads(encoder, file_ctx->flac_threads);
	if (ret != FLAC__STREAM_ENCODER_SET_NUM_THREADS_OK) {
		fprintf(stderr, "ERROR: failed to set FLAC threads: %s\n", _FLAC_StreamEncoderSetNumThreadsStatusString[ret]);
	}
#endif
	if((seektable = FLAC__metadata_object_new(FLAC__METADATA_TYPE_SEEKTABLE)) == NULL
		|| FLAC__metadata_object_seektable_template_append_spaced_points(seektable, 1<<18, (uint64_t)1<<41) != true
		|| FLAC__stream_encoder_set_metadata(encoder, &seektable, 1) != true) {
		fprintf(stderr, "ERROR: could not create FLAC seektable\n");
		do_exit = 1;
		return 0;
	}

	init_status = FLAC__stream_encoder_init_FILE(encoder, file_ctx->f, NULL, NULL);
	if(init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
		fprintf(stderr, "ERROR: failed initializing FLAC encoder: %s\n", FLAC__StreamEncoderInitStatusString[init_status]);
		do_exit = 1;
		return 0;
	}

	while(true) {
		while(((buf = rb_read_ptr(&file_ctx->rb, len)) == NULL) && !do_exit) {
			thrd_sleep(&(struct timespec){.tv_nsec=10000000}, NULL);
		}
		if (do_exit) {
			len = file_ctx->rb.tail - file_ctx->rb.head;
			if (len == 0) break;
			buf = rb_read_ptr(&file_ctx->rb, len);
		}
#if LIBSWRESAMPLE_ENABLED == 1
		if (file_ctx->resample_rate!=0) {
			swr_ret = swr_convert(swr_ctx, (uint8_t **)&resample_buffer, len>>2, (const uint8_t **)&buf, len>>2);
			if (swr_ret < 0) {
				fprintf(stderr, "Error while converting\n");
				do_exit = 1;
				return 0;
			}
			ok = FLAC__stream_encoder_process(encoder, (const FLAC__int32**)&resample_buffer, swr_ret);
		} else {
			ok = FLAC__stream_encoder_process(encoder, (const FLAC__int32**)&buf, len>>2);
		}
#else
		ok = FLAC__stream_encoder_process(encoder, (const FLAC__int32**)&buf, len>>2);
#endif
		if(!ok) {
			fprintf(stderr, "ERROR: (%p) FLAC encoder could not process data: %s\n", file_ctx->f, FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(encoder)]);
			new_line = 1;
		}
		rb_read_finished(&file_ctx->rb, len);
	}
	FLAC__metadata_object_seektable_template_sort(seektable, false);
	/* bug in libflac < 1.5, fix seektable manually */
#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT < 14
	for(int i = seektable->data.seek_table.num_points-1; i>=0; i--) {
		if (seektable->data.seek_table.points[i].stream_offset != 0) break;
		seektable->data.seek_table.points[i].sample_number = 0xFFFFFFFFFFFFFFFF;
	}
#endif
	ok = FLAC__stream_encoder_finish(encoder);
	if(!ok) {
		fprintf(stderr, "ERROR: FLAC encoder did not finish correctly: %s\n", FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(encoder)]);
		new_line = 1;
		return 0;
	}
	FLAC__metadata_object_delete(seektable);
	FLAC__stream_encoder_delete(encoder);
#if LIBSWRESAMPLE_ENABLED == 1
	if (file_ctx->resample_rate!=0) {
		aligned_free(resample_buffer);
		swr_free(&swr_ctx);
	}
#endif
	return 0;
}
#endif

int open_file(FILE **f, char *filename, bool overwrite)
{
	if (strcmp(filename, "-") == 0) { // Write to stdout
		*f = stdout;
		return 0;
	}
	if (access(filename, F_OK) == 0 && !overwrite) {
		char ch = 0;
		fprintf(stderr, "File '%s' already exists. Overwrite? (y/n) ", filename);
		scanf(" %c",&ch);
		if (ch != 'y' && ch != 'Y') return -1;
	}
	*f = fopen(filename, "wb");
	if (!(*f)) {
		fprintf(stderr, "Failed to open %s\n", filename);
		return -2;
	}
	return 0;
}

int main(int argc, char **argv)
{
//set pipe mode to binary in windows
#if defined(_WIN32) || defined(_WIN64)
	_setmode(_fileno(stdout), O_BINARY);
	_setmode(_fileno(stdin), O_BINARY);
#else
	struct sigaction sigact;
#endif

	int r, opt, pad=0, dev_index=0, out_size = 2;
#if LIBFLAC_ENABLED == 1
	int flac_level = 1;
	bool flac_verify = false;
	bool flac_12bit = false;
	uint32_t flac_threads = 0;
#endif
#if LIBSWRESAMPLE_ENABLED == 1
	uint32_t resample_rate[] = {0,0};
#endif
	thrd_start_t output_thread_func = (thrd_start_t)raw_file_writer;
	capture_ctx_t cap_ctx;
	memset(&cap_ctx,0,sizeof(cap_ctx));

	// getopt string
	char getopt_string[256];

	// device names
	char dev_manufact[256];
	char dev_product[256];
	char dev_serial[256];

	//output threads
	// out 1, 2
	thrd_t thread_out[2] = { 0, 0 };
	thrd_t thread_audio = 0;
	filewriter_ctx_t thread_out_ctx[2];
	audiowriter_ctx_t thread_audio_ctx;
	char outbuffer_name[] = "outX_ringbuffer";

	//file adress
	// out 1, 2
	char *output_names[2] = { NULL, NULL };
	char *output_name_aux = NULL;
	char *output_name_raw = NULL;

	char *output_name_4ch_audio = NULL;
	char *output_names_2ch_audio[2] = { NULL, NULL };
	char *output_names_1ch_audio[4] = { NULL, NULL, NULL, NULL };

	//show clipping messages
	bool suppress_a_clipping = false;
	bool suppress_b_clipping = false;

	//overwrite option
	bool overwrite_files = false;

	//number of samples to take
	uint64_t total_samples_before_exit = 0;

	//output files
	FILE *output_aux = NULL;
	FILE *output_raw = NULL;

	//buffer
	uint8_t  *buf_aux = aligned_alloc(16,sizeof(uint8_t) *BUFFER_READ_SIZE);

	uint64_t total_samples = 0;

	//clipping state
	size_t clip[2] = {0, 0};

	// conversion function
	conv_function_t conv_function;

	memset(&thread_audio_ctx, 0, sizeof(audiowriter_ctx_t));

	fprintf(stderr,
		"MISRC capture " VERSION "\n"
		COPYRIGHT "\n\n"
	);

	create_getopt_string(getopt_string);

	int index_ptr;
	while ((opt = getopt_long(argc, argv, getopt_string, getopt_long_options, &index_ptr)) != -1) {
		switch (opt) {
		case 'd':
			dev_index = (uint32_t)atoi(optarg);
			break;
		case 'a':
			output_names[0] = optarg;
			break;
		case 'b':
			output_names[1] = optarg;
			break;
#if LIBFLAC_ENABLED == 1
#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 14
		case 'c':
			flac_threads = (uint32_t)atoi(optarg);
			break;
#endif
		case OPT_RF_FLAC_12BIT:
			flac_12bit = true;
			break;
		case 'f':
			output_thread_func = (thrd_start_t)flac_file_writer;
			out_size = 4;
			break;
		case 'l':
			flac_level = (uint32_t)atoi(optarg);
			break;
		case 'v':
			flac_verify = true;
			break;
#endif
		case 'x':
			output_name_aux = optarg;
			break;
		case 'r':
			output_name_raw = optarg;
			break;
		case 'p':
			pad = 1;
			break;
		case 'w':
			overwrite_files = true;
			break;
		case 'n':
			total_samples_before_exit = (uint64_t)strtoull(optarg,NULL,0);
			break;
		case 't':
			if(total_samples_before_exit == 0) {
				char *tp;
				tp = strtok(optarg, ":");
				while (tp != NULL) {
					total_samples_before_exit *= 60;
					total_samples_before_exit += (uint64_t)strtoull(tp,NULL,10);
					tp = strtok(NULL, ":");
				}
				total_samples_before_exit *= 40000000;
			}
			break;
		case 'A':
			suppress_a_clipping = true;
			break;
		case 'B':
			suppress_b_clipping = true;
			break;
#if LIBSWRESAMPLE_ENABLED == 1
		case OPT_RESAMPLE_A:
			resample_rate[0] = (uint32_t)atoi(optarg);
			break;
		case OPT_RESAMPLE_B:
			resample_rate[1] = (uint32_t)atoi(optarg);
			break;
#endif
		case OPT_AUDIO_4CH_OUT:
			output_name_4ch_audio = optarg;
			break;
		case OPT_AUDIO_2CH_12_OUT:
			output_names_2ch_audio[0] = optarg;
			break;
		case OPT_AUDIO_2CH_34_OUT:
			output_names_2ch_audio[1] = optarg;
			break;
		case OPT_AUDIO_1CH_1_OUT:
			output_names_1ch_audio[0] = optarg;
			break;
		case OPT_AUDIO_1CH_2_OUT:
			output_names_1ch_audio[1] = optarg;
			break;
		case OPT_AUDIO_1CH_3_OUT:
			output_names_1ch_audio[2] = optarg;
			break;
		case OPT_AUDIO_1CH_4_OUT:
			output_names_1ch_audio[3] = optarg;
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}

	if(output_names[0] == NULL && output_names[1] == NULL && output_name_aux == NULL && output_name_raw == NULL) {
		usage();
	}
	else {
		cap_ctx.capture_rf = true;
	}

#if LIBSWRESAMPLE_ENABLED == 1
	if(resample_rate[0] >= 40000 || resample_rate[1] >= 40000) {
		fprintf(stderr, "ERROR: Resampling to rates higher than 40 MHz is not supported!\n");
		usage();
	}
#endif

#if LIBFLAC_ENABLED == 1
	if(flac_12bit && pad == 1) {
		fprintf(stderr, "Warning: You enabled padding the lower 4 bits, but requested 12 bit flac output, this is not possible, will output 16 bit flac.\n");
		flac_12bit = false;
	}
# if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 14
	if (flac_threads == 0) {
		int out_cnt = ((output_names[0] == NULL) ? 0 : 1) + ((output_names[1] == NULL) ? 0 : 1);
		if (out_cnt != 0) {
			flac_threads = get_num_cores();
			fprintf(stderr,"Detected %d cores in the system available to the process\n",flac_threads);
			flac_threads = (flac_threads - 2 - out_cnt) / out_cnt;
			if (flac_threads == 0) flac_threads = 1;
			if (flac_threads > 128) flac_threads = 128;
		}
	}
# endif
#endif

	if(suppress_a_clipping) {
		fprintf(stderr, "Suppressing clipping messages from ADC A\n");
	}
	if(suppress_b_clipping) {
		fprintf(stderr, "Suppressing clipping messages from ADC B\n");
	}
	if(total_samples_before_exit > 0) {
		fprintf(stderr, "Capturing %" PRIu64 " samples before exiting\n", total_samples_before_exit);
	}


#ifndef _WIN32
	sigact.sa_handler = sighandler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
	sigaction(SIGTERM, &sigact, NULL);
	sigaction(SIGQUIT, &sigact, NULL);
	sigaction(SIGPIPE, &sigact, NULL);
#else
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE) sighandler, true );
#endif
	for(int i=0; i<2; i++) {
		if (output_names[i] != NULL) {
			if (open_file(&(thread_out_ctx[i].f),output_names[i],overwrite_files)) return -ENOENT;
#if LIBFLAC_ENABLED == 1
			thread_out_ctx[i].flac_level = flac_level;
			thread_out_ctx[i].flac_verify = flac_verify;
			thread_out_ctx[i].flac_threads = flac_threads;
			thread_out_ctx[i].flac_bits = flac_12bit ? 12 : 16;
#endif
#if LIBSWRESAMPLE_ENABLED == 1
			thread_out_ctx[i].resample_rate = resample_rate[i];
#endif
			outbuffer_name[3] = (char)(i+48);
			rb_init(&thread_out_ctx[i].rb, outbuffer_name, BUFFER_TOTAL_SIZE);
			r = thrd_create(&thread_out[i], output_thread_func, &thread_out_ctx[i]);
			if (r != thrd_success) {
				fprintf(stderr, "Failed to create thread for output processing\n");
				return -ENOENT;
			}
		}
	}

	if(output_name_4ch_audio != NULL)
	{
		//opening output file audio
		if (open_file(&(thread_audio_ctx.f_4ch), output_name_4ch_audio, overwrite_files)) return -ENOENT;
		cap_ctx.capture_audio = true;
	}

	for(int i=0; i<2; i++) {
		if(output_names_2ch_audio[i] != NULL)
		{
			//opening output file audio
			if (open_file(&(thread_audio_ctx.f_2ch[i]), output_names_2ch_audio[i], overwrite_files)) return -ENOENT;
			cap_ctx.capture_audio = true;
		}
	}

	for(int i=0; i<4; i++) {
		if(output_names_1ch_audio[i] != NULL)
		{
			//opening output file audio
			if (open_file(&(thread_audio_ctx.f_1ch[i]), output_names_1ch_audio[i], overwrite_files)) return -ENOENT;
			cap_ctx.capture_audio = true;
		}
	}

	if(output_name_aux != NULL)
	{
		//opening output file aux
		if (open_file(&output_aux, output_name_aux, overwrite_files)) return -ENOENT;
	}

	if(output_name_raw != NULL)
	{
		//opening output file raw
		if (open_file(&output_raw, output_name_raw, overwrite_files)) return -ENOENT;
	}

	if(cap_ctx.capture_audio) {
		rb_init(&cap_ctx.rb_audio,"capture_audio_ringbuffer",BUFFER_AUDIO_TOTAL_SIZE);
		thread_audio_ctx.rb = &cap_ctx.rb_audio;
		r = thrd_create(&thread_audio, &audio_file_writer, &thread_audio_ctx);
		if (r != thrd_success) {
			fprintf(stderr, "Failed to create thread for output processing\n");
			return -ENOENT;
		}
	}

	conv_function = get_conv_function(0, pad, (out_size==2) ? 0 : 1, output_names[0], output_names[1]);

	rb_init(&cap_ctx.rb,"capture_ringbuffer",BUFFER_TOTAL_SIZE);

	r = hsdaoh_alloc(&dev);
	if (r < 0) {
		fprintf(stderr, "Failed to allocate hsdaoh device.\n");
		exit(1);
	}

	hsdaoh_raw_callback(dev, true);
	hsdaoh_set_msg_callback(dev, &print_capture_message, NULL);

	r = hsdaoh_open2(dev, (uint32_t)dev_index);
	if (r < 0) {
		fprintf(stderr, "Failed to open hsdaoh device #%d.\n", dev_index);
		exit(1);
	}

	dev_manufact[0] = 0;
	dev_product[0] = 0;
	dev_serial[0] = 0;
	r = hsdaoh_get_usb_strings(dev, dev_manufact, dev_product, dev_serial);
	if (r < 0)
		fprintf(stderr, "Failed to identify hsdaoh device #%d.\n", dev_index);
	else
		fprintf(stderr, "Opened device #%d: %s %s, serial: %s\n", dev_index, dev_manufact, dev_product, dev_serial);

	fprintf(stderr, "Reading samples...\n");
	r = hsdaoh_start_stream(dev, hsdaoh_callback, &cap_ctx);

	while (!do_exit) {
		void *buf, *buf_out1, *buf_out2;
		while((((buf = rb_read_ptr(&cap_ctx.rb, BUFFER_READ_SIZE*4)) == NULL) || 
			  (output_names[0] != NULL && ((buf_out1 = rb_write_ptr(&thread_out_ctx[0].rb, BUFFER_READ_SIZE*out_size)) == NULL)) ||
			  (output_names[1] != NULL && ((buf_out2 = rb_write_ptr(&thread_out_ctx[1].rb, BUFFER_READ_SIZE*out_size)) == NULL))) && 
			  !do_exit)
		{
			sleep_ms(10);
		}
		if (do_exit) break;
		conv_function((uint32_t*)buf, BUFFER_READ_SIZE, clip, buf_aux, buf_out1, buf_out2);
		if(output_raw != NULL){fwrite(buf,4,BUFFER_READ_SIZE,output_raw);}
		rb_read_finished(&cap_ctx.rb, BUFFER_READ_SIZE*4);
		if(output_aux != NULL){fwrite(buf_aux,1,BUFFER_READ_SIZE,output_aux);}
		if(output_names[0] != NULL) rb_write_finished(&thread_out_ctx[0].rb, BUFFER_READ_SIZE*out_size);
		if(output_names[1] != NULL) rb_write_finished(&thread_out_ctx[1].rb, BUFFER_READ_SIZE*out_size);

		total_samples += BUFFER_READ_SIZE;

		if(clip[0] > 0 && !suppress_a_clipping)
		{
			fprintf(stderr,"ADC A : %zu samples clipped\n",clip[0]);
			clip[0] = 0;
			new_line = 1;
		}

		if(clip[1] > 0 && !suppress_b_clipping)
		{
			fprintf(stderr,"ADC B : %zu samples clipped\n",clip[1]);
			clip[1] = 0;
			new_line = 1;
		}
		if (total_samples % (BUFFER_READ_SIZE<<2) == 0) {
			if(new_line) fprintf(stderr,"\n");
			new_line = 0;
			fprintf(stderr,"\033[A\33[2K\r Progress: %13" PRIu64 " samples, %2uh %2um %2us\n", total_samples, (uint32_t)(total_samples/(144000000000)), (uint32_t)((total_samples/(2400000000)) % 60), (uint32_t)((total_samples/(40000000)) % 60));
			fflush(stderr);
		}
		if (total_samples >= total_samples_before_exit && total_samples_before_exit != 0) {
			fprintf(stderr, "%" PRIu64 " total samples have been collected, exiting early!\n", total_samples);
			do_exit = true;
		}
	}

	if (do_exit)
		fprintf(stderr, "\nUser cancel, exiting...\n");
	else
		fprintf(stderr, "\nLibrary error %d, exiting...\n", r);

	hsdaoh_close(dev);

////ending of the program

	aligned_free(buf_aux);

	if (output_aux && (output_aux != stdout)) fclose(output_aux);
	if (output_raw && (output_raw != stdout)) fclose(output_raw);

	for(int i=0;i<2;i++) {
		if (thread_out[i]!=0) {
			r = thrd_join(thread_out[i], NULL);
			if (r != thrd_success) fprintf(stderr, "Failed to join thread %d.\n", i);
		}
	}

	if (thread_audio!=0) {
		r = thrd_join(thread_audio, NULL);
		if (r != thrd_success) fprintf(stderr, "Failed to join audio thread.\n");
	}

	return 0;
}
