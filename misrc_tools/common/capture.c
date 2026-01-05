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
#include <pthread.h>
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
#include <stdarg.h>
#if __STDC_VERSION__ >= 201112L && ! __STDC_NO_THREADS__ && ! _WIN32
#include <threads.h>
#else
#include "cthreads.h"
#warning "No C threads, fallback to pthreads/winthreads"
#endif
#include <time.h>

#ifndef _WIN32
	#if defined(__APPLE__) || defined(__MACH__)
		#include <libkern/OSByteOrder.h>
		#define le32toh(x) OSSwapLittleToHostInt32(x)
		#define le16toh(x) OSSwapLittleToHostInt16(x)
	#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
		#include <sys/endian.h>
		#define le32toh(x) letoh32(x)
		#define le16toh(x) letoh16(x)
	#else
		#include <endian.h>
	#endif
	#include <unistd.h>
	#define aligned_free(x) free(x)
	#define sleep_ms(x) usleep(x*1000)
#else
	#include <windows.h>
	#include <io.h>
	#include <fcntl.h>
	#define aligned_free(x) _aligned_free(x)
	#define aligned_alloc(a,s) _aligned_malloc(s,a)
	#define sleep_ms(x) Sleep(x)
	#define F_OK 0
	#define access _access
	#define le32toh(x) (x)
	#define le16toh(x) (x)
#endif

#include "simple_capture/simple_capture.h"
#include "misrc.h"
#include "misrc_options.h"
//#include "version.h"
#include "ringbuffer.h"
#include "extract.h"
#include "wave.h"

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

#if LIBSOXR_ENABLED == 1
#include <soxr.h>
#endif

#if LIBFLAC_ENABLED == 1 && defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 14
#include "numcores.h"
#endif

#define _FILE_OFFSET_BITS 64

#if defined(__GNUC__)
# define UNUSED(x) x __attribute__((unused))
#else
# define UNUSED(x) x
#endif

typedef struct {
	misrc_settings_t *set;
	ringbuffer_t rb;
	ringbuffer_t rb_audio;
	int hsdaoh_frames_since_error;
	unsigned int hsdaoh_in_order_cnt;
	unsigned int non_sync_cnt;
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
	misrc_settings_t *set;
	ringbuffer_t rb;
	FILE *f;
	int idx;
#if LIBSOXR_ENABLED == 1
	conv_16to32_t conv_func;
	double init_scale;
	double resample_rate;
	uint32_t resample_qual;
	float resample_gain;
	bool reduce_8bit;
#endif
#if LIBFLAC_ENABLED == 1
	uint32_t flac_level;
	bool flac_verify;
	uint32_t flac_threads;
	uint8_t flac_bits;
#endif
} filewriter_ctx_t;

typedef struct {
	misrc_settings_t *set;
	ringbuffer_t *rb;
	FILE *f_4ch;
	FILE *f_2ch[2];
	FILE *f_1ch[4];
	uint64_t total_bytes;
	bool non_4ch;
} audiowriter_ctx_t;

static int do_exit;
static hsdaoh_dev_t *hs_dev = NULL;
static sc_handle_t *sc_dev = NULL;
static conv_16to32_t conv_16to32 = NULL;
static conv_16to32_t conv_16to8to32 = NULL;
static conv_16to32_t conv_16to12to32 = NULL;
static conv_16to8_t conv_16to8 = NULL;

static void hsdaoh_callback(hsdaoh_data_info_t *data_info)
{
	capture_ctx_t *cap_ctx = data_info->ctx;
	metadata_t meta;
	misrc_sync_info_t sync_info;
	uint32_t stream0_payload_bytes = 0;
	uint32_t stream1_payload_bytes = 0;
	int frame_errors = 0;
	uint8_t *buf_out;
	uint8_t *buf_out_audio;

	if (do_exit)
		return;

	if(cap_ctx) {
		hsdaoh_extract_metadata(data_info->buf, &meta, data_info->width);

		if(!cap_ctx->hsdaoh_stream_synced) {
#if defined(__linux__) && defined(_GNU_SOURCE)
			pthread_setname_np(pthread_self(), "hsdaoh_frame");
#endif
			if(cap_ctx->set->count_cb) cap_ctx->set->count_cb(cap_ctx->set->count_cb_ctx, MISRC_COUNT_NONSYNC_FRAMES, cap_ctx->non_sync_cnt+1); 
			if(cap_ctx->non_sync_cnt == 500) {
				if(cap_ctx->set->msg_cb) cap_ctx->set->msg_cb(cap_ctx->set->msg_cb_ctx,MISRC_MSG_ERROR,"Received more than 500 corrupted frames! Check connection!");
				if (sc_dev) if(cap_ctx->set->msg_cb) cap_ctx->set->msg_cb(cap_ctx->set->msg_cb_ctx,MISRC_MSG_ERROR,"Verify that your device does not modify the video data!");
			}
		}

		if (le32toh(meta.magic) != HSDAOH_MAGIC) {
			if (cap_ctx->hsdaoh_stream_synced) {
				if(cap_ctx->set->msg_cb) cap_ctx->set->msg_cb(cap_ctx->set->msg_cb_ctx,MISRC_MSG_ERROR,"Lost sync to HDMI input stream");
			}
			cap_ctx->hsdaoh_stream_synced = false;
			cap_ctx->non_sync_cnt++;
			return;
		}

		/* drop duplicated frames */
		if (meta.framecounter == cap_ctx->hsdaoh_last_frame_cnt)
			return;

		if (meta.framecounter != ((cap_ctx->hsdaoh_last_frame_cnt + 1) & 0xffff)) {
			cap_ctx->hsdaoh_in_order_cnt = 0;
			if (cap_ctx->hsdaoh_stream_synced)
				if(cap_ctx->set->msg_cb) cap_ctx->set->msg_cb(cap_ctx->set->msg_cb_ctx,MISRC_MSG_ERROR,"Missed at least one frame, fcnt %d, expected %d!",
					meta.framecounter, ((cap_ctx->hsdaoh_last_frame_cnt + 1) & 0xffff));
		} else
			cap_ctx->hsdaoh_in_order_cnt++;

		cap_ctx->hsdaoh_last_frame_cnt = meta.framecounter;

		if (cap_ctx->capture_rf) while((buf_out = rb_write_ptr(&cap_ctx->rb, data_info->len))==NULL) {
			if (do_exit) return;
			if(cap_ctx->set->msg_cb) cap_ctx->set->msg_cb(cap_ctx->set->msg_cb_ctx,MISRC_MSG_WARNING,"Cannot get space in ringbuffer for next frame (RF)");
			sleep_ms(4);
		}

		if (cap_ctx->capture_audio) while((buf_out_audio = rb_write_ptr(&cap_ctx->rb_audio, data_info->len))==NULL) {
			if (do_exit) return;
			if(cap_ctx->set->msg_cb) cap_ctx->set->msg_cb(cap_ctx->set->msg_cb_ctx,MISRC_MSG_WARNING,"Cannot get space in ringbuffer for next frame (audio)");
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
					if(cap_ctx->set->msg_cb) cap_ctx->set->msg_cb(cap_ctx->set->msg_cb_ctx,MISRC_MSG_ERROR,"Invalid payload length: %d", payload_len);
					/* discard frame */
					return;
				}
				cap_ctx->non_sync_cnt++;
				return;
			}

			uint16_t idle_len = (data_info->width-1) - payload_len - ((meta.flags & FLAG_STREAM_ID_PRESENT) ? 1 : 0) - ((meta.crc_config == CRC_NONE) ? 0 : 1);
			frame_errors += hsdaoh_check_idle_cnt(&cap_ctx->hsdaoh_idle_cnt, (uint16_t *)line_dat + payload_len, idle_len);

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
					stream0_payload_bytes += payload_len * sizeof(uint16_t);
				}
				else if (cap_ctx->capture_audio && stream_id == 1) {
					if(cap_ctx->capture_audio_started2) {
						memcpy(buf_out_audio + stream1_payload_bytes, line_dat, payload_len * sizeof(uint16_t));
						stream1_payload_bytes += payload_len * sizeof(uint16_t);
					}
					else {
						if (cap_ctx->capture_audio_started) {
							cap_ctx->capture_audio_started2 = true;
							if(cap_ctx->set->msg_cb) cap_ctx->set->msg_cb(cap_ctx->set->msg_cb_ctx,MISRC_MSG_INFO,"Audio and RF now in sync");
						}
						else
							cap_ctx->capture_audio_started = true;
					}
				}
			}
		}

		if (frame_errors && cap_ctx->hsdaoh_stream_synced) {
			if(cap_ctx->set->msg_cb) cap_ctx->set->msg_cb(cap_ctx->set->msg_cb_ctx,MISRC_MSG_ERROR,"%d frame errors, %d frames since last error", frame_errors, cap_ctx->hsdaoh_frames_since_error);
			cap_ctx->hsdaoh_frames_since_error = 0;
		} else {
			cap_ctx->hsdaoh_frames_since_error++;
			if (cap_ctx->capture_rf) rb_write_finished(&cap_ctx->rb, stream0_payload_bytes);
			if (cap_ctx->capture_audio) rb_write_finished(&cap_ctx->rb_audio, stream1_payload_bytes);
		}
		if (!cap_ctx->hsdaoh_stream_synced && !frame_errors && (cap_ctx->hsdaoh_in_order_cnt > 4)) {
			//if(cap_ctx->set->msg_cb) cap_ctx->set->msg_cb(cap_ctx->set->msg_cb_ctx, MISRC_MSG_INFO, "Syncronized to HDMI input stream\n MISRC uses CRC: %s\n MISRC uses stream ids: %s",
			//						yesno[((meta.crc_config == CRC_NONE) ? 0 : 1)], yesno[((meta.flags & FLAG_STREAM_ID_PRESENT) ? 1 : 0)]);
			if(cap_ctx->set->sync_cb) {
				sync_info.use_crc = (meta.crc_config != CRC_NONE);
				sync_info.use_stream_id = (meta.flags & FLAG_STREAM_ID_PRESENT);
				cap_ctx->set->sync_cb(cap_ctx->set->sync_cb_ctx, &sync_info);
			}
			if (cap_ctx->capture_audio) {
				if ((meta.flags & FLAG_STREAM_ID_PRESENT)) {
					if(cap_ctx->set->msg_cb) cap_ctx->set->msg_cb(cap_ctx->set->msg_cb_ctx,MISRC_MSG_INFO,"Wait for RF and audio syncronisation...");
				}
				else {
					if(cap_ctx->set->msg_cb) cap_ctx->set->msg_cb(cap_ctx->set->msg_cb_ctx,MISRC_MSG_CRITICAL,"MISRC does not transmit audio, cannot capture audio!");
					do_exit = 1;
					return;
				}
			}
			cap_ctx->hsdaoh_stream_synced = true;
			cap_ctx->non_sync_cnt = 0;
		}
	}
}

static int audio_file_writer(void *ctx)
{
	audiowriter_ctx_t *audio_ctx = ctx;
	size_t len = BUFFER_AUDIO_READ_SIZE;
	void *buf;
	wave_header_t h;
	bool convert_1ch = false;
	bool convert_2ch = false;
	uint8_t* buffer_1ch[4];
	uint8_t* buffer_2ch[2];
#if defined(__linux__) && defined(_GNU_SOURCE)
	pthread_setname_np(pthread_self(), "out_audio");
#endif
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


static int raw_file_writer(void *ctx)
{
	const char rfidx[] = { 'A', 'B' };
	filewriter_ctx_t *file_ctx = ctx;
	size_t len = BUFFER_READ_SIZE;
	void *buf;
#if defined(__linux__) && defined(_GNU_SOURCE)
	char thread_name[] = "out_RAW_RF_X";
	thread_name[11] = rfidx[file_ctx->idx];
	pthread_setname_np(pthread_self(), thread_name);
#endif
#if LIBSOXR_ENABLED == 1
	/* setup resampling */
	uint8_t *resample_buffer;
	uint8_t *resample_buffer_b;
	soxr_t resampler = NULL;
	soxr_error_t soxr_err;
	if (file_ctx->resample_rate!=0.0) {
		soxr_io_spec_t io_spec = soxr_io_spec(SOXR_INT16_S, SOXR_INT16_S);
		soxr_quality_spec_t qual_spec = soxr_quality_spec(file_ctx->resample_qual, 0);
		io_spec.scale = file_ctx->init_scale;
		io_spec.scale *= pow(10.0,file_ctx->resample_gain/20.0);
		resample_buffer = aligned_alloc(32, BUFFER_READ_SIZE);
		resample_buffer_b = aligned_alloc(32, BUFFER_READ_SIZE);
		if (!resample_buffer || !resample_buffer_b) {
			if(file_ctx->set->msg_cb) file_ctx->set->msg_cb(file_ctx->set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed allocating resampling buffer");
			do_exit = 1;
			return 0;
		}
		resampler = soxr_create(40000.0, file_ctx->resample_rate, 1, &soxr_err, &io_spec, &qual_spec, NULL);
		if (!resampler || soxr_err!=0) {
			if(file_ctx->set->msg_cb) file_ctx->set->msg_cb(file_ctx->set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed allocating resampling context: %s", soxr_err);
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
#if LIBSOXR_ENABLED == 1
		if (file_ctx->resample_rate!=0) {
			size_t out_len;
			soxr_err = soxr_process(resampler, &buf, len>>1, &len, &resample_buffer, len>>1, &out_len);
			len<<=1;
			if (soxr_err != 0) {
				if(file_ctx->set->msg_cb) file_ctx->set->msg_cb(file_ctx->set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Error while resampling: %s", soxr_err);
				do_exit = 1;
				return 0;
			}
			if (file_ctx->reduce_8bit) {
				conv_16to8((int16_t*)resample_buffer, (int8_t*)resample_buffer_b, out_len);
				fwrite(resample_buffer_b, 1, out_len, file_ctx->f);
			}
			else {
				fwrite(resample_buffer, 1, out_len<<1, file_ctx->f);
			}
		} else {
			fwrite(buf, 1, len, file_ctx->f);
		}
#else
		fwrite(buf, 1, len, file_ctx->f);
#endif
		rb_read_finished(&file_ctx->rb, len);
	}
	if (file_ctx->f != stdout) fclose(file_ctx->f);
#if LIBSOXR_ENABLED == 1
	if (file_ctx->resample_rate!=0) {
		aligned_free(resample_buffer);
		aligned_free(resample_buffer_b);
		soxr_delete(resampler);
	}
#endif
	return 0;
}

#if LIBFLAC_ENABLED == 1
int flac_file_writer(void *ctx)
{
	const char rfidx[] = { 'A', 'B' };
	filewriter_ctx_t *file_ctx = ctx;
	size_t len = BUFFER_READ_SIZE;
	void *buf;
	uint32_t ret;
	uint32_t srate = 40000;
	FLAC__bool ok = true;
	FLAC__StreamEncoder *encoder = NULL;
	FLAC__StreamEncoderInitStatus init_status;
	FLAC__StreamMetadata *seektable;

#if defined(__linux__) && defined(_GNU_SOURCE)
	char thread_name[] = "out_FLAC_RF_X";
	thread_name[12] = rfidx[file_ctx->idx];
	pthread_setname_np(pthread_self(), thread_name);
#endif

#if LIBSOXR_ENABLED == 1
	uint8_t *resample_buffer;
	uint8_t *resample_buffer_b;
	soxr_t resampler = NULL;
	soxr_error_t soxr_err;
	if (file_ctx->resample_rate!=0.0) {
		srate = (uint32_t)(file_ctx->resample_rate);
		resample_buffer = aligned_alloc(32, BUFFER_READ_SIZE);
		resample_buffer_b = aligned_alloc(32, BUFFER_READ_SIZE);
		soxr_io_spec_t io_spec = soxr_io_spec(SOXR_INT32_S, SOXR_INT16_S);
		soxr_quality_spec_t qual_spec = soxr_quality_spec(file_ctx->resample_qual, 0);
		io_spec.scale = file_ctx->init_scale;
		io_spec.scale *= pow(10.0,file_ctx->resample_gain/20.0);
		if (!resample_buffer || !resample_buffer_b) {
			if(file_ctx->set->msg_cb) file_ctx->set->msg_cb(file_ctx->set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed allocating resampling buffer");
			do_exit = 1;
			return 0;
		}
		resampler = soxr_create(40000.0, file_ctx->resample_rate, 1, &soxr_err, &io_spec, &qual_spec, NULL);
		if (!resampler || soxr_err!=0) {
			if(file_ctx->set->msg_cb) file_ctx->set->msg_cb(file_ctx->set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed allocating resampling context: %s", soxr_err);
			do_exit = 1;
			return 0;
		}
	}
#endif

	if((encoder = FLAC__stream_encoder_new()) == NULL) {
		if(file_ctx->set->msg_cb) file_ctx->set->msg_cb(file_ctx->set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed allocating FLAC encoder");
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
		if(file_ctx->set->msg_cb) file_ctx->set->msg_cb(file_ctx->set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed initializing FLAC encoder");
		do_exit = 1;
		return 0;
	}
#if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 14
	ret = FLAC__stream_encoder_set_num_threads(encoder, file_ctx->flac_threads);
	if (ret != FLAC__STREAM_ENCODER_SET_NUM_THREADS_OK) {
		if(file_ctx->set->msg_cb) file_ctx->set->msg_cb(file_ctx->set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed to set FLAC threads: %s", _FLAC_StreamEncoderSetNumThreadsStatusString[ret]);
	}
#endif
	if((seektable = FLAC__metadata_object_new(FLAC__METADATA_TYPE_SEEKTABLE)) == NULL
		|| FLAC__metadata_object_seektable_template_append_spaced_points(seektable, 1<<18, (uint64_t)1<<41) != true
		|| FLAC__stream_encoder_set_metadata(encoder, &seektable, 1) != true) {
		if(file_ctx->set->msg_cb) file_ctx->set->msg_cb(file_ctx->set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Could not create FLAC seektable");
		do_exit = 1;
		return 0;
	}

	init_status = FLAC__stream_encoder_init_FILE(encoder, file_ctx->f, NULL, NULL);
	if(init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
		if(file_ctx->set->msg_cb) file_ctx->set->msg_cb(file_ctx->set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed initializing FLAC encoder: %s", FLAC__StreamEncoderInitStatusString[init_status]);
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
#if LIBSOXR_ENABLED == 1
		if (file_ctx->resample_rate!=0) {
			size_t out_len;
			soxr_err = soxr_process(resampler, &buf, len>>2, &len, &resample_buffer, len>>1, &out_len);
			len<<=2;
			if (soxr_err != 0) {
				if(file_ctx->set->msg_cb) file_ctx->set->msg_cb(file_ctx->set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Error while converting: %s", soxr_err);
				do_exit = 1;
				return 0;
			}
			file_ctx->conv_func((int16_t*)resample_buffer, (int32_t*)resample_buffer_b, out_len);
			ok = FLAC__stream_encoder_process(encoder, (const FLAC__int32**)&resample_buffer_b, out_len);
		} else {
			ok = FLAC__stream_encoder_process(encoder, (const FLAC__int32**)&buf, len>>2);
		}
#else
		ok = FLAC__stream_encoder_process(encoder, (const FLAC__int32**)&buf, len>>2);
#endif
		if(!ok) {
			if(file_ctx->set->msg_cb) file_ctx->set->msg_cb(file_ctx->set->msg_cb_ctx, MISRC_MSG_CRITICAL, "(RF %c) FLAC encoder could not process data: %s", rfidx[file_ctx->idx], FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(encoder)]);
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
		if(file_ctx->set->msg_cb) file_ctx->set->msg_cb(file_ctx->set->msg_cb_ctx, MISRC_MSG_CRITICAL, "(RF %c) FLAC encoder did not finish correctly: %s", rfidx[file_ctx->idx], FLAC__StreamEncoderStateString[FLAC__stream_encoder_get_state(encoder)]);
		return 0;
	}
	FLAC__metadata_object_delete(seektable);
	FLAC__stream_encoder_delete(encoder);
#if LIBSOXR_ENABLED == 1
	if (file_ctx->resample_rate!=0) {
		aligned_free(resample_buffer);
		aligned_free(resample_buffer_b);
		soxr_delete(resampler);
	}
#endif
	return 0;
}
#endif

static int open_file(FILE **f, char *filename, misrc_settings_t *set)
{
	if (strcmp(filename, "-") == 0) { // Write to stdout
		*f = stdout;
		return 0;
	}
	if (access(filename, F_OK) == 0 && !set->overwrite_files) {
		if (!set->overwrite_cb || !set->overwrite_cb(set->overwrite_cb_ctx, filename)) return MISRC_RET_USER_ABORT;
	}
	*f = fopen(filename, "wb");
	if (!(*f)) {
		set->msg_cb(set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed to open %s", filename);
		return MISRC_RET_FILE_ERROR;
	}
	return 0;
}

static bool str_starts_with(const char *restrict prefixA, const char *restrict prefixB, size_t *prefixLen, const char *restrict string)
{
	while(*prefixA) {
		if(prefixLen) (*prefixLen)++;
		if(*prefixA++ != *string++)
			return false;
	}
	if (prefixB) while(*prefixB) {
		if(prefixLen) (*prefixLen)++;
		if(*prefixB++ != *string++)
			return false;
	}
	return true;
}

void misrc_capture_set_default(misrc_settings_t *set, misrc_option_t *opt)
{
	while(opt->short_opt!=0) {
		switch(opt->arg_type) {
		case MISRC_ARGTYPE_BOOL:
			for(int i=0;i<mirsc_opt_type_cnt[opt->opt_type];i++) MISRC_SET_OPTION(bool,set,opt,i,opt->default_arg.i==1);
			break;
		case MISRC_ARGTYPE_INT:
		case MISRC_ARGTYPE_LIST:
			for(int i=0;i<mirsc_opt_type_cnt[opt->opt_type];i++) MISRC_SET_OPTION(int64_t,set,opt,i,opt->default_arg.i);
			break;
		case MISRC_ARGTYPE_FLOAT:
			for(int i=0;i<mirsc_opt_type_cnt[opt->opt_type];i++) MISRC_SET_OPTION(double,set,opt,i,opt->default_arg.f);
			break;
		default:
			for(int i=0;i<mirsc_opt_type_cnt[opt->opt_type];i++) MISRC_SET_OPTION(char*,set,opt,i,opt->default_arg.s);
			break;
		}
		opt++;
	}
}

char* misrc_sc_capture_impl_name()
{
	return sc_get_impl_name();
}

void misrc_list_devices(misrc_device_info_t **dev_info, size_t *n) {
	sc_capture_dev_t *sc_devs;
	size_t sc_n = sc_get_devices(&sc_devs);
	uint32_t hs_n = hsdaoh_get_device_count();
	*dev_info = malloc(sizeof(misrc_device_info_t)*(sc_n+hs_n));

	for (*n=0; *n<hs_n; (*n)++) {
		char *d = malloc(4);
		snprintf(d,4,"%li",*n);
		(*dev_info)[*n] = (misrc_device_info_t){MISRC_DEV_HSDAOH, d, hsdaoh_get_device_name(*n)};
	}

	for (size_t i=0; i<sc_n; i++) {
		sc_formatlist_t *sc_fmt;
		size_t f_n = sc_get_formats(sc_devs[i].device_id, &sc_fmt);
		for (size_t j=0; j<f_n; j++) {
			if (SC_CODEC_EQUAL(sc_fmt[j].codec, SC_CODEC_YUYV)) for (size_t k=0; k<sc_fmt[j].n_sizes; k++) {
				if (sc_fmt[j].sizes[k].w == 1920 && sc_fmt[j].sizes[k].h == 1080) for (size_t l=0; l<sc_fmt[j].sizes[k].n_fps; l++) {
					if ((sc_fmt[j].sizes[k].fps[l].den != 0 && (float)sc_fmt[j].sizes[k].fps[l].num / (float)sc_fmt[j].sizes[k].fps[l].den >= 40.0f)
					|| (sc_fmt[j].sizes[k].fps[l].den == 0 && (float)sc_fmt[j].sizes[k].fps[l].max_num / (float)sc_fmt[j].sizes[k].fps[l].max_den >= 40.0f)) {
						char *d = malloc(256);
						snprintf(d,256, "%s://%s",sc_get_impl_name_short(), sc_devs[i].device_id);
						(*dev_info)[*n] = (misrc_device_info_t){MISRC_DEV_GENERIC_SC, d, strdup(sc_devs[i].name)};
						(*n)++;
						break;
					}
				}
			}
		}
	}
}

void misrc_stop_capture()
{
	do_exit = 1;
}

int misrc_run_capture(misrc_settings_t *set)
{
//set pipe mode to binary in windows
#if defined(_WIN32) || defined(_WIN64)
	_setmode(_fileno(stdout), O_BINARY);
	_setmode(_fileno(stdin), O_BINARY);
#endif
	const int64_t resample_qual_list[] = { SOXR_QQ, SOXR_LQ, SOXR_MQ, SOXR_HQ, SOXR_VHQ };

	int r, dev_index = 0, out_size = 2;
	size_t str_cnt = 0;

	thrd_start_t output_thread_func = (thrd_start_t)raw_file_writer;
	capture_ctx_t cap_ctx;
	memset(&cap_ctx,0,sizeof(cap_ctx));

	// device names
	char dev_manufact[256];
	char dev_product[256];
	char dev_serial[256];

	char *sc_dev_name = NULL;

	//output threads
	// out 1, 2
	thrd_t thread_out[2] = { 0, 0 };
	thrd_t thread_audio = 0;
	filewriter_ctx_t thread_out_ctx[2];
	audiowriter_ctx_t thread_audio_ctx;
	char outbuffer_name[] = "outX_ringbuffer";

	//aux buffer
	uint8_t  *buf_aux = aligned_alloc(16,sizeof(uint8_t) *BUFFER_READ_SIZE);

	uint64_t total_samples = 0;

	//output files
	FILE *output_aux = NULL;
	FILE *output_raw = NULL;

	//clipping state
	size_t clip[2] = {0, 0};
	//peak level
	uint16_t peak_level[2] = {0, 0};

	// conversion function
	conv_function_t conv_function;

	memset(&thread_audio_ctx, 0, sizeof(audiowriter_ctx_t));

	cap_ctx.capture_rf = true;
	cap_ctx.set = set;

	if (str_starts_with(sc_get_impl_name_short(), "://", &str_cnt, set->device)) {
		sc_dev_name = strdup(&(set->device[str_cnt]));
	}
	else
		dev_index = (int)atoi(set->device);

#if LIBFLAC_ENABLED == 1
	if(set->flac_12bit && set->flac_bits == 0) set->flac_bits = 1;
	if(set->flac_enable) {
		output_thread_func = (thrd_start_t)flac_file_writer;
		out_size = 4;
		if (set->pad == 1) {
			if(set->flac_bits == 1) {
				set->msg_cb(set->msg_cb_ctx, MISRC_MSG_CRITICAL, "You enabled padding the lower 4 bits, but requested 12 bit flac output, this is not possible!");
				return MISRC_RET_INVALID_SETTINGS;
			}
			set->flac_bits = 2;
		}
		else {
			if(set->flac_bits != 2) set->flac_bits = 1;
		}
# if defined(FLAC_API_VERSION_CURRENT) && FLAC_API_VERSION_CURRENT >= 14
		if (set->flac_threads == 0) {
			int out_cnt = ((set->output_names_rf[0] == NULL) ? 0 : 1) + ((set->output_names_rf[1] == NULL) ? 0 : 1);
			if (out_cnt != 0) {
				set->flac_threads = get_num_cores();
				set->msg_cb(set->msg_cb_ctx, MISRC_MSG_INFO, "Detected %d cores in the system available to the process",set->flac_threads);
				set->flac_threads = (set->flac_threads - 2 - out_cnt) / out_cnt;
				if (set->flac_threads == 0) set->flac_threads = 1;
				if (set->flac_threads > 128) set->flac_threads = 128;
			}
		}
# endif
	}
#endif

#if LIBSOXR_ENABLED == 1
	for(int i=0; i<2; i++) {
		set->resample_qual[i] = resample_qual_list[set->resample_qual[i]];
		if (set->resample_rate[i] == 40000.0) set->resample_rate[i] = 0.0;
	}
	if(set->resample_rate[0] > 40000.0 || set->resample_rate[1] > 40000.0) {
		set->msg_cb(set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Upsampling to higher frequencies than 40 kHz is not supported!");
		return MISRC_RET_INVALID_SETTINGS;
	}
	if((set->resample_rate[0] != 0.0 || set->resample_rate[1] != 0.0) && out_size == 4) {
# if LIBFLAC_ENABLED == 1
		if (set->flac_bits == 1) {
			conv_16to12to32 = get_16to12to32_function();
		} else {
# endif
			conv_16to32 = get_16to32_function();
# if LIBFLAC_ENABLED == 1
		}
# endif
	}
	if(set->reduce_8bit[0] || set->reduce_8bit[1]) {
		if (out_size == 4) 
			conv_16to8to32 = get_16to8to32_function();
		else 
			conv_16to8 = get_16to8_function();
		if(set->reduce_8bit[0] && set->resample_rate[0]==0.0) set->resample_rate[0] = 40000.0;
		if(set->reduce_8bit[1] && set->resample_rate[1]==0.0) set->resample_rate[1] = 40000.0;
	}
#endif

	for(int i=0; i<2; i++) {
		if (set->output_names_rf[i] != NULL) {
			if (open_file(&(thread_out_ctx[i].f), set->output_names_rf[i],set)) return -ENOENT;
			if (out_size == 4) {
				thread_out_ctx[i].init_scale = (set->reduce_8bit[i]) ? ((set->pad) ? 256.0 : 4096.0) : 65536.0;
			} else {
				thread_out_ctx[i].init_scale = (set->reduce_8bit[i]) ? ((set->pad) ? 0.00390625 : 0.0625) : 1.0;
			}
			thread_out_ctx[i].idx = i;
			thread_out_ctx[i].set = set;
#if LIBFLAC_ENABLED == 1
			thread_out_ctx[i].flac_level = set->flac_level;
			thread_out_ctx[i].flac_verify = set->flac_verify;
			thread_out_ctx[i].flac_threads = set->flac_threads;
#if LIBSOXR_ENABLED == 1
			thread_out_ctx[i].flac_bits = set->reduce_8bit[i] ? 8 : ((set->flac_bits == 1) ? 12 : 16);
			thread_out_ctx[i].conv_func = set->reduce_8bit[i] ? conv_16to8to32 : ((set->flac_bits == 1) ? conv_16to12to32 : conv_16to32);
#else
			thread_out_ctx[i].flac_bits = (set->flac_bits == 1) ? 12 : 16;
#endif
#endif
#if LIBSOXR_ENABLED == 1
			thread_out_ctx[i].reduce_8bit = set->reduce_8bit[i];
			thread_out_ctx[i].resample_rate = set->resample_rate[i];
			thread_out_ctx[i].resample_qual = set->resample_qual[i];
			thread_out_ctx[i].resample_gain = set->resample_gain[i];
#endif
			outbuffer_name[3] = (char)(i+48);
			rb_init(&thread_out_ctx[i].rb, outbuffer_name, BUFFER_TOTAL_SIZE);
			r = thrd_create(&thread_out[i], output_thread_func, &thread_out_ctx[i]);
			if (r != thrd_success) {
				return MISRC_RET_THREAD_ERROR;
			}
		}
	}

	if(set->output_name_4ch_audio != NULL)
	{
		//opening output file audio
		if (open_file(&(thread_audio_ctx.f_4ch), set->output_name_4ch_audio, set)) return -ENOENT;
		cap_ctx.capture_audio = true;
	}

	for(int i=0; i<2; i++) {
		if(set->output_names_2ch_audio[i] != NULL)
		{
			//opening output file audio
			if (open_file(&(thread_audio_ctx.f_2ch[i]), set->output_names_2ch_audio[i], set)) return -ENOENT;
			cap_ctx.capture_audio = true;
		}
	}

	for(int i=0; i<4; i++) {
		if(set->output_names_1ch_audio[i] != NULL)
		{
			//opening output file audio
			if (open_file(&(thread_audio_ctx.f_1ch[i]), set->output_names_1ch_audio[i], set)) return -ENOENT;
			cap_ctx.capture_audio = true;
		}
	}

	if(set->output_name_aux != NULL)
	{
		//opening output file aux
		if (open_file(&output_aux, set->output_name_aux, set)) return -ENOENT;
	}

	if(set->output_name_raw != NULL)
	{
		//opening output file raw
		if (open_file(&output_raw, set->output_name_raw, set)) return -ENOENT;
	}

	if(cap_ctx.capture_audio) {
		rb_init(&cap_ctx.rb_audio,"capture_audio_ringbuffer",BUFFER_AUDIO_TOTAL_SIZE);
		thread_audio_ctx.rb = &cap_ctx.rb_audio;
		r = thrd_create(&thread_audio, &audio_file_writer, &thread_audio_ctx);
		if (r != thrd_success) {
			set->msg_cb(set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed to create thread for output processing");
			return MISRC_RET_THREAD_ERROR;
		}
	}

	conv_function = get_conv_function(0, set->pad, (out_size==2) ? 0 : 1, set->calc_level, set->output_names_rf[0], set->output_names_rf[1]);

	rb_init(&cap_ctx.rb,"capture_ringbuffer",BUFFER_TOTAL_SIZE);

	if (sc_dev_name) {
		r = sc_start_capture(sc_dev_name, 1920, 1080, SC_CODEC_YUYV, 60, 1, (sc_frame_callback_t)hsdaoh_callback, &cap_ctx, &sc_dev);
		if (r < 0) {
			set->msg_cb(set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed to open %s device %s.", sc_get_impl_name(), sc_dev_name);
			return MISRC_RET_HARDWARE_ERROR;
		}
		set->msg_cb(set->msg_cb_ctx, MISRC_MSG_INFO, "Opened %s device %s.\n", sc_get_impl_name(), sc_dev_name);
	}
	else {

#if defined(__linux__) && defined(_GNU_SOURCE)
		pthread_setname_np(pthread_self(), "hsdaoh");
#endif

		r = hsdaoh_alloc(&hs_dev);
		if (r < 0) {
			set->msg_cb(set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed to allocate hsdaoh device.");
			return MISRC_RET_MEMORY_ERROR;
		}

		hsdaoh_raw_callback(hs_dev, true);
		/* directly passing the message handler is not ideal, but as
		   the MISRC changes to hsdaoh are probably never merged anyway it doesn't matter */
		hsdaoh_set_msg_callback(hs_dev, (hsdaoh_message_cb_t)set->msg_cb, set->msg_cb_ctx);

		r = hsdaoh_open2(hs_dev, (uint32_t)dev_index);
		if (r < 0) {
			set->msg_cb(set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed to open hsdaoh device #%d.", dev_index);
			return MISRC_RET_HARDWARE_ERROR;
		}

		dev_manufact[0] = 0;
		dev_product[0] = 0;
		dev_serial[0] = 0;
		r = hsdaoh_get_usb_strings(hs_dev, dev_manufact, dev_product, dev_serial);
		if (r < 0)
			set->msg_cb(set->msg_cb_ctx, MISRC_MSG_ERROR, "Failed to identify hsdaoh device #%d.", dev_index);
		else
			set->msg_cb(set->msg_cb_ctx, MISRC_MSG_INFO, "Opened hsdaoh device #%d: %s %s, serial: %s", dev_index, dev_manufact, dev_product, dev_serial);

		set->msg_cb(set->msg_cb_ctx, MISRC_MSG_INFO, "Reading samples...");
		r = hsdaoh_start_stream(hs_dev, hsdaoh_callback, &cap_ctx);
	}

#if defined(__linux__) && defined(_GNU_SOURCE)
		pthread_setname_np(pthread_self(), "misrc_cap_main");
#endif

	while (!do_exit) {
		void *buf, *buf_out1 = NULL, *buf_out2 = NULL;
		while((((buf = rb_read_ptr(&cap_ctx.rb, BUFFER_READ_SIZE*4)) == NULL) || 
			  (set->output_names_rf[0] != NULL && ((buf_out1 = rb_write_ptr(&thread_out_ctx[0].rb, BUFFER_READ_SIZE*out_size)) == NULL)) ||
			  (set->output_names_rf[1] != NULL && ((buf_out2 = rb_write_ptr(&thread_out_ctx[1].rb, BUFFER_READ_SIZE*out_size)) == NULL))) && 
			  !do_exit)
		{
			sleep_ms(10);
		}
		if (do_exit) break;
		conv_function((uint32_t*)buf, BUFFER_READ_SIZE, clip, buf_aux, buf_out1, buf_out2, peak_level);
		if(output_raw != NULL){fwrite(buf,4,BUFFER_READ_SIZE,output_raw);}
		rb_read_finished(&cap_ctx.rb, BUFFER_READ_SIZE*4);
		if(output_aux != NULL){fwrite(buf_aux,1,BUFFER_READ_SIZE,output_aux);}
		if(set->output_names_rf[0] != NULL) rb_write_finished(&thread_out_ctx[0].rb, BUFFER_READ_SIZE*out_size);
		if(set->output_names_rf[1] != NULL) rb_write_finished(&thread_out_ctx[1].rb, BUFFER_READ_SIZE*out_size);

		total_samples += BUFFER_READ_SIZE;

		if (set->stats_cb) set->stats_cb(set->stats_cb_ctx, total_samples, clip, peak_level);

		if (total_samples >= set->total_samples_before_exit && set->total_samples_before_exit != 0) {
			if (set->count_cb) set->count_cb(set->count_cb_ctx, MISRC_COUNT_TOTAL_SAMPLES_END, total_samples);
			do_exit = true;
		}
	}

	if (do_exit)
		set->msg_cb(set->msg_cb_ctx, MISRC_MSG_INFO, "User cancel, exiting...");
	else
		set->msg_cb(set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Library error %d, exiting...", r);

	if (hs_dev) { hsdaoh_close(hs_dev); hs_dev = NULL; }
	if (sc_dev) { sc_stop_capture(sc_dev); sc_dev = NULL; }

////ending of the program

	aligned_free(buf_aux);

	if (output_aux && (output_aux != stdout)) fclose(output_aux);
	if (output_raw && (output_raw != stdout)) fclose(output_raw);

	for(int i=0;i<2;i++) {
		if (thread_out[i]!=0) {
			r = thrd_join(thread_out[i], NULL);
			if (r != thrd_success) set->msg_cb(set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed to join thread %d.", i);
		}
	}

	if (thread_audio!=0) {
		r = thrd_join(thread_audio, NULL);
		if (r != thrd_success) set->msg_cb(set->msg_cb_ctx, MISRC_MSG_CRITICAL, "Failed to join audio thread.");
	}

	return MISRC_RET_CAPTURE_OK;
}
