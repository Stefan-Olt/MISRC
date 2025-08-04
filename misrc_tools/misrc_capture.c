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
#endif

#include <hsdaoh.h>

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

#if LIBFLAC_ENABLED == 1 && defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 14
#include "numcores.h"
#endif

#define VERSION "0.4"
#define COPYRIGHT "licensed under GNU GPL v3 or later, (c) 2024-2025 vrunk11, stefan_o"

#define BUFFER_TOTAL_SIZE 65536*1024
#define BUFFER_READ_SIZE 65536*32

#define _FILE_OFFSET_BITS 64

#define OPT_RESAMPLE_A     256
#define OPT_RESAMPLE_B     257
#define OPT_RF_FLAC_12BIT  258

#if defined(__GNUC__)
# define UNUSED(x) x __attribute__((unused))
#else
# define UNUSED(x) x
#endif

typedef struct {
	ringbuffer_t rb;
	uint64_t samples_to_read;
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


static int do_exit;
static int new_line = 1;
static hsdaoh_dev_t *dev = NULL;

static struct option getopt_long_options[] =
{
  {"device",               required_argument, 0, 'd'},
  {"count",                required_argument, 0, 'n'},
  {"time",                 required_argument, 0, 't'},
  {"overwrite",            no_argument,       0, 'w'},
  {"adc-a-output",         required_argument, 0, 'a'},
  {"adc-b-output",         required_argument, 0, 'b'},
  {"aux-output",           required_argument, 0, 'x'},
  {"raw-output",           required_argument, 0, 'r'},
  {"pad",                  no_argument,       0, 'p'},
  {"suppress-clip-a",      no_argument,       0, 'A'},
  {"suppress-clip-b",      no_argument,       0, 'B'},
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
  {0, 0, 0, 0}
};

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

static void hsdaoh_callback(unsigned char *buf, uint32_t len, uint8_t UNUSED(pack_state), void *ctx)
{
	capture_ctx_t *cap_ctx = ctx;
	if (ctx) {
		if (do_exit)
			return;
		//len >>= 2;
		if ((cap_ctx->samples_to_read > 0) && (cap_ctx->samples_to_read < len)) {
			len = cap_ctx->samples_to_read;
			do_exit = 1;
			hsdaoh_stop_stream(dev);
		}
		while(rb_put(&cap_ctx->rb,buf,len&(~0x3))) {
			if (do_exit) return;
			fprintf(stderr,"Cannot write frame to buffer\n");
			new_line = 1;
			sleep_ms(4);
		}
		if (cap_ctx->samples_to_read > 0)
			cap_ctx->samples_to_read -= len;
	}
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

void print_hsdaoh_message(int msg_type, int msg, void *additional, void UNUSED(*ctx))
{
	hsdaoh_get_message_string(msg_type, msg, additional, NULL);
	new_line = 1;
}

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
	filewriter_ctx_t thread_out_ctx[2];
	char outbuffer_name[] = "outX_ringbuffer";

	//file adress
	// out 1, 2
	char *output_names[2] = { NULL, NULL };
	char *output_name_aux = NULL;
	char *output_name_raw = NULL;

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
		case 'h':
		default:
			usage();
			break;
		}
	}

	if(output_names[0] == NULL && output_names[1] == NULL && output_name_aux == NULL && output_name_raw == NULL) {
		usage();
	}

#if LIBSWRESAMPLE_ENABLED == 1
	if(resample_rate[0] > 40000 || resample_rate[1] > 40000) {
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
			if (open_file(&(thread_out_ctx[i].f), output_names[i], overwrite_files)) return -ENOENT;
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

	conv_function = get_conv_function(0, pad, (out_size==2) ? 0 : 1, output_names[0], output_names[1]);

	rb_init(&cap_ctx.rb,"capture_ringbuffer",BUFFER_TOTAL_SIZE);

	r = hsdaoh_open_msg_cb(&dev, (uint32_t)dev_index, &print_hsdaoh_message, NULL);
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
			fflush(stdout);
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

	return 0;
}
