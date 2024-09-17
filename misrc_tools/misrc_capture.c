/*
* MISRC capture
* Copyright (C) 2024  vrunk11, stefan_o
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

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#ifndef _WIN32
	#include <getopt.h>
	#include <unistd.h>
	#define aligned_free(x) free(x)
#else
	#include <windows.h>
	#include <io.h>
	#include <fcntl.h>
	#include "getopt/getopt.h"
	#define aligned_free(x) _aligned_free(x)
	#define aligned_alloc(a,s) _aligned_malloc(s,a)
#endif

#include <hsdaoh.h>
#include "extract.h"

#define VERSION "0.1"
#define COPYRIGHT "licensed under GNU GPL v3 or later, (c) 2024 vrunk11, stefan_o"

#define BUFFER_SIZE 65536*32

#define _FILE_OFFSET_BITS 64

typedef struct {
	FILE *output_1;
	FILE *output_2;
	FILE *output_aux;
	FILE *output_raw;
	//buffer
	int16_t  *buf_1;
	int16_t  *buf_2;
	uint8_t  *buf_aux;
	//clipping state
	size_t clip[2];
	// conversion function
	conv_function_t conv_function;
	uint64_t samples_to_read;
} capture_ctx_t;

static int do_exit;
static hsdaoh_dev_t *dev = NULL;

void usage(void)
{
	fprintf(stderr,
		"A simple program to capture from MISRC using hsdaoh\n\n"
		"Usage:\n"
		"\t[-d device_index (default: 0)]\n"
		"\t[-n number of samples to read (default: 0, infinite)]\n"
		"\t[-a ADC A output file (use '-' to write on stdout)]\n"
		"\t[-b ADC B output file (use '-' to write on stdout)]\n"
		"\t[-x AUX output file (use '-' to write on stdout)]\n"
		"\t[-r raw data output file (use '-' to write on stdout)]\n"
		"\t[-p pad lower 4 bits of 16 bit output with 0 instead of upper 4]\n"
	);
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
		return TRUE;
	}
	return FALSE;
}
#else
static void sighandler(int signum)
{
	signal(SIGPIPE, SIG_IGN);
	fprintf(stderr, "Signal caught, exiting!\n");
	do_exit = 1;
	hsdaoh_stop_stream(dev);
}
#endif

static void hsdaoh_callback(unsigned char *buf, uint32_t len, uint8_t pack_state, void *ctx)
{
	capture_ctx_t *cap_ctx = ctx;
	if (cap_ctx) {
		if (do_exit)
			return;
		len >>= 2;
		if ((cap_ctx->samples_to_read > 0) && (cap_ctx->samples_to_read < len)) {
			len = cap_ctx->samples_to_read;
			do_exit = 1;
			hsdaoh_stop_stream(dev);
		}
		cap_ctx->conv_function((uint32_t*)buf, len, cap_ctx->clip, cap_ctx->buf_aux, cap_ctx->buf_1, cap_ctx->buf_2);
		if(cap_ctx->clip[0] > 0)
		{
			fprintf(stderr,"ADC A : %ld samples clipped\n",cap_ctx->clip[0]);
			cap_ctx->clip[0] = 0; 
		}

		if(cap_ctx->clip[1] > 0)
		{
			fprintf(stderr,"ADC B : %ld samples clipped\n",cap_ctx->clip[1]);
			cap_ctx->clip[1] = 0; 
		}
		//write output
		if(cap_ctx->output_1   != NULL){fwrite(cap_ctx->buf_1, 2,len,cap_ctx->output_1);}
		if(cap_ctx->output_2   != NULL){fwrite(cap_ctx->buf_2, 2,len,cap_ctx->output_2);}
		if(cap_ctx->output_aux != NULL){fwrite(cap_ctx->buf_aux,1,len,cap_ctx->output_aux);}
		if(cap_ctx->output_raw != NULL){fwrite(buf,4,len,cap_ctx->output_raw);}

		if (cap_ctx->samples_to_read > 0)
			cap_ctx->samples_to_read -= len;
	}
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

	int r, opt, pad=0, dev_index=0;

	capture_ctx_t cap_ctx;
	memset(&cap_ctx,0,sizeof(cap_ctx));

	//file adress
	char *output_name_1   = NULL;
	char *output_name_2   = NULL;
	char *output_name_aux = NULL;
	char *output_name_raw = NULL;

	//buffer
	cap_ctx.buf_1   = aligned_alloc(16,sizeof(int16_t) *BUFFER_SIZE);
	cap_ctx.buf_2   = aligned_alloc(16,sizeof(int16_t) *BUFFER_SIZE);
	cap_ctx.buf_aux = aligned_alloc(16,sizeof(uint8_t) *BUFFER_SIZE);

	//number of byte read
	int nb_block;

	fprintf(stderr,
		"MISRC capture " VERSION "\n"
		COPYRIGHT "\n\n"
	);

	while ((opt = getopt(argc, argv, "d:a:b:x:r:psh")) != -1) {
		switch (opt) {
		case 'd':
			dev_index = (uint32_t)atoi(optarg);
			break;
		case 'a':
			output_name_1 = optarg;
			break;
		case 'b':
			output_name_2 = optarg;
			break;
		case 'x':
			output_name_aux = optarg;
			break;
		case 'r':
			output_name_raw = optarg;
			break;
		case 'p':
			pad = 1;
			break; 1;
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}

	if(output_name_1 == NULL && output_name_2 == NULL && output_name_aux == NULL && output_name_raw == NULL)
	{
		usage();
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
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE) sighandler, TRUE );
#endif

	if(output_name_1 != NULL)
	{
		//opening output file 1
		if (strcmp(output_name_1, "-") == 0)// Write to stdout
		{
			cap_ctx.output_1 = stdout;
		}
		else
		{
			cap_ctx.output_1 = fopen(output_name_1, "wb");
			if (!cap_ctx.output_1) {
				fprintf(stderr, "(2) : Failed to open %s\n", output_name_1);
				return -ENOENT;
			}
		}
	}

	if(output_name_2 != NULL)
	{
		//opening output file 2
		if (strcmp(output_name_2, "-") == 0)// Write to stdout
		{
			cap_ctx.output_2 = stdout;
		}
		else if(output_name_2 != NULL)
		{
			cap_ctx.output_2 = fopen(output_name_2, "wb");
			if (!cap_ctx.output_2) {
				fprintf(stderr, "(2) : Failed to open %s\n", output_name_2);
				return -ENOENT;
			}
		}
	}

	if(output_name_aux != NULL)
	{
		//opening output file aux
		if (strcmp(output_name_aux, "-") == 0)// Write to stdout
		{
			cap_ctx.output_aux = stdout;
		}
		else if(output_name_aux != NULL)
		{
			cap_ctx.output_aux = fopen(output_name_aux, "wb");
			if (!cap_ctx.output_aux) {
				fprintf(stderr, "(2) : Failed to open %s\n", output_name_aux);
				return -ENOENT;
			}
		}
	}

	if(output_name_raw != NULL)
	{
		//opening output file aux
		if (strcmp(output_name_raw, "-") == 0)// Write to stdout
		{
			cap_ctx.output_raw = stdout;
		}
		else if(output_name_raw != NULL)
		{
			cap_ctx.output_raw = fopen(output_name_raw, "wb");
			if (!cap_ctx.output_raw) {
				fprintf(stderr, "(2) : Failed to open %s\n", output_name_raw);
				return -ENOENT;
			}
		}
	}


	cap_ctx.conv_function = get_conv_function(0, pad, output_name_1, output_name_2);


	r = hsdaoh_open(&dev, (uint32_t)dev_index);
	if (r < 0) {
		fprintf(stderr, "Failed to open hsdaoh device #%d.\n", dev_index);
		exit(1);
	}

	fprintf(stderr, "Reading samples...\n");
	r = hsdaoh_start_stream(dev, hsdaoh_callback, &cap_ctx);


	while (!do_exit) {
		usleep(50000);
	}

	if (do_exit)
		fprintf(stderr, "\nUser cancel, exiting...\n");
	else
		fprintf(stderr, "\nLibrary error %d, exiting...\n", r);

	hsdaoh_close(dev);

////ending of the program

	aligned_free(cap_ctx.buf_1);
	aligned_free(cap_ctx.buf_2);
	aligned_free(cap_ctx.buf_aux);


	if(output_name_1 != NULL)
	{
		//Close out file
		if (cap_ctx.output_1 && (cap_ctx.output_1 != stdout))
		{
			fclose(cap_ctx.output_1);
		}
	}

	if(output_name_2 != NULL)
	{
		if (cap_ctx.output_2 && (cap_ctx.output_2 != stdout))
		{
			fclose(cap_ctx.output_2);
		}
	}

	if(output_name_aux != NULL)
	{
		if (cap_ctx.output_aux && (cap_ctx.output_aux != stdout))
		{
			fclose(cap_ctx.output_aux);
		}
	}

	if(output_name_raw != NULL)
	{
		if (cap_ctx.output_raw && (cap_ctx.output_raw != stdout))
		{
			fclose(cap_ctx.output_raw);
		}
	}
	return 0;
}
