/*
* simple_capture_test
* Copyright (C) 2025 stefan_o
* Some parts of this code are AI-generated
* 
* This program will test the simple capture library
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "simple_capture.h"

/* State for writing frames */
typedef struct {
	FILE* f;
	volatile int frames;
	int max_frames;
} FrameWriter;

static void write_frame_cb(sc_data_info_t *data_info)
{
	FrameWriter* fw = (FrameWriter*)data_info->ctx;
	if (!fw || !fw->f) return;
	if (fw->frames >= fw->max_frames) return;

	/* Write raw bytes as delivered by the API (may include stride padding) */
	size_t wrote = fwrite(data_info->data, 1, data_info->len, fw->f);
	(void)wrote; /* ignore short write in this tiny example */
	fw->frames++;
}

static int parse_fourcc_to_codec(const char* s, sc_codec_t* out) {
	char up[5] = {0};
	size_t i;
	for (i = 0; i < 4 && s[i]; ++i) {
		up[i] = (char)toupper((unsigned char)s[i]);
	}
	if (i < 4) return 0;  // need at least 4 chars

	if (strncmp(up, "YUYV", 4) == 0 || strncmp(up, "YUY2", 4) == 0 || strncmp(up, "YUV2", 4) == 0) { SC_CODEC_SET(*out, SC_CODEC_YUYV); return 1; }
	if (strncmp(up, "UYVY", 4) == 0) { SC_CODEC_SET(*out, SC_CODEC_UYVY); return 1; }
#ifdef SC_CODEC_YVYU
	if (strncmp(up, "YVYU", 4) == 0) { SC_CODEC_SET(*out, SC_CODEC_YVYU); return 1; }
#endif
#ifdef SC_CODEC_VYUY
	if (strncmp(up, "VYUY", 4) == 0) { SC_CODEC_SET(*out, SC_CODEC_VYUY); return 1; }
#endif
	if (strncmp(up, "GREY", 4) == 0) { SC_CODEC_SET(*out, SC_CODEC_GREY); return 1; }
	return 0;
}

static void sleep_ms(unsigned ms) {
#ifdef _MSC_VER
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

const char* codec_to_name(sc_codec_t c) {
	if (SC_CODEC_EQUAL(c,SC_CODEC_GREY)) {
		return "Grey 8-Bit / GREY";
	}
	if (SC_CODEC_EQUAL(c,SC_CODEC_RGB24)) {
		return "RGB 8-Bit / RGB24";
	}
	if (SC_CODEC_EQUAL(c,SC_CODEC_BGR24)) {
		return "RGB 8-Bit / BGR24";
	}
	if (SC_CODEC_EQUAL(c,SC_CODEC_MJPEG)) {
		return "M-JPEG / MJPG";
	}
	if (SC_CODEC_EQUAL(c,SC_CODEC_YUYV)) {
		return "YUV 4:2:2 8-Bit / YUVV (YUV2)";
	}
	if (SC_CODEC_EQUAL(c,SC_CODEC_UYVY)) {
		return "YUV 4:2:2 8-Bit / UYVY";
	}
#ifdef SC_CODEC_YVYU
	if (SC_CODEC_EQUAL(c,SC_CODEC_YVYU)) {
		return "YUV 4:2:2 8-Bit / YVYU";
	}
#endif
#ifdef SC_CODEC_VYUY
	if (SC_CODEC_EQUAL(c,SC_CODEC_VYUY)) {
		return "YUV 4:2:2 8-Bit / VYUY";
	}
#endif
	return "Unknown";
}

int main(int argc, char** argv) {
	sc_capture_dev_t *devs;
	size_t dev_n = sc_get_devices(&devs);
	for (size_t i=0; i<dev_n;i++) {
		sc_formatlist_t *fmts;
		fprintf(stderr,"Device: %s, Name: %s\n", devs[i].device_id, devs[i].name);
		size_t fmt_n = sc_get_formats(devs[i].device_id, &fmts);
		for (size_t j=0; j<fmt_n; j++) {
			fprintf(stderr," Got format: %s\n", codec_to_name(fmts[j].codec));
			for (size_t k=0; k<fmts[j].n_sizes; k++) {
				fprintf(stderr,"  W: %i H: %i\n", fmts[j].sizes[k].w, fmts[j].sizes[k].h);
				for (size_t l=0; l<fmts[j].sizes[k].n_fps; l++) {
					if (fmts[j].sizes[k].fps[l].num==0) {
						fprintf(stderr,"   %i/%i ... + %i/%i ... %i/%i\n", fmts[j].sizes[k].fps[l].min_num, fmts[j].sizes[k].fps[l].min_den, fmts[j].sizes[k].fps[l].step_num, fmts[j].sizes[k].fps[l].step_den, fmts[j].sizes[k].fps[l].max_num, fmts[j].sizes[k].fps[l].max_den);
					}
					else {
						fprintf(stderr,"   %i/%i\n", fmts[j].sizes[k].fps[l].num, fmts[j].sizes[k].fps[l].den);
					}
				}
			}
		}
	}

	if (argc < 8) return 0;

	const char* device_id = argv[1];
	const char* fourcc = argv[2];
	unsigned width = (unsigned)strtoul(argv[3], NULL, 10);
	unsigned height = (unsigned)strtoul(argv[4], NULL, 10);
	int fps_num = (int)strtol(argv[5], NULL, 10);
	int fps_den = (int)strtol(argv[6], NULL, 10);
	const char* out_path = argv[7];

	if (!device_id || !fourcc || width == 0 || height == 0 || fps_den == 0 || !out_path) {
		fprintf(stderr, "Invalid arguments\n");
		return 1;
	}

	sc_codec_t codec;
	if (!parse_fourcc_to_codec(fourcc, &codec)) {
		fprintf(stderr, "Unsupported FourCC '%s'. Use YUYV, UYVY, YVYU, or VYUY.\n", fourcc);
		return 1;
	}

	FILE* f = fopen(out_path, "wb");
	if (!f) {
		perror("fopen");
		return 1;
	}

	FrameWriter fw = {0};
	fw.f = f;
	fw.frames = 0;
	fw.max_frames = 500;

	sc_handle_t* cap_handle = NULL;
	int rc = sc_start_capture(device_id, width, height, codec, fps_num, fps_den, write_frame_cb, &fw, &cap_handle);
	if (rc != 0 || !cap_handle) {
		fprintf(stderr, "start_capture failed\n");
		fclose(f);
		return 1;
	}

	/* Busy-wait until 100 frames written */
	while (fw.frames < fw.max_frames) {
		sleep_ms(10);
	}

	sc_stop_capture(cap_handle);
	fclose(f);

	fprintf(stdout, "Captured %d frames to %s\n", fw.frames, out_path);
	return 0;

}
