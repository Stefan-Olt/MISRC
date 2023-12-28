/*
* MISRC extract
* Copyright (C) 2023  vrunk11, stefan_o
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <getopt.h>
#include <unistd.h>
#include <stdint.h>

#ifndef _WIN32
	#include <unistd.h>
	#define sleep_ms(ms)	usleep(ms*1000)
	#else
	#include <windows.h>
	#include <io.h>
	#include <fcntl.h>
	#define sleep_ms(ms)	Sleep(ms)
#endif

#define PERF_MEASURE 1

#ifdef PERF_MEASURE
#include <time.h>
#endif

#define BUFFER_SIZE 65536*32

#define _FILE_OFFSET_BITS 64

#ifdef _WIN64
#define FSEEK fseeko64
#else
#define FSEEK fseeko
#endif

void usage(void)
{
	fprintf(stderr,
		"MISRC extract, a simple program for extracting captured data into separate file\n\n"
		"Usage:\n"
		"\t[-i input file (use '-' to read from stdin)]\n"
		"\t[-a ADC A output file (use '-' to write on stdout)]\n"
		"\t[-b ADC B output file (use '-' to write on stdout)]\n"
		"\t[-x AUX output file (use '-' to write on stdout)]\n"
	);
	exit(1);
}

//bit masking
#define MASK_1      0xFFF
#define MASK_2      0xFFF00000
#define MASK_AUX    0xFF000

void extract_A_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = ((int16_t)(in[i] & MASK_1)) - 2048;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}
void extract_B_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB) {
	for(size_t i = 0; i < len; i++)
	{
		outB[i]  = ((int16_t)((in[i] & MASK_2) >> 20)) - 2048;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_AB_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = ((int16_t)(in[i] & MASK_1)) - 2048;
		outB[i]  = ((int16_t)((in[i] & MASK_2) >> 20)) - 2048;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		clip[1] += ((in[i] >> 13) & 1);
	}
}

#if defined(__x86_64__) || defined(_M_X64)
void extract_A_sse (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_B_sse (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_AB_sse(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
int check_cpu_feat();
#endif

int main(int argc, char **argv)
{
//set pipe mode to binary in windows
#if defined(_WIN32) || defined(_WIN64)
	_setmode(_fileno(stdout), O_BINARY);
	_setmode(_fileno(stdin), O_BINARY);
#endif

	int opt;
	
	//file adress
	char *input_name_1    = NULL;
	char *output_name_1   = NULL;
	char *output_name_2   = NULL;
	char *output_name_aux = NULL;
	
	//input file
	FILE *input_1;
	
	//output files
	FILE *output_1;
	FILE *output_2;
	FILE *output_aux;
	
	//buffer
	uint32_t *buf_tmp = aligned_alloc(16,sizeof(uint32_t)*BUFFER_SIZE);
	int16_t  *buf_1   = aligned_alloc(16,sizeof(int16_t) *BUFFER_SIZE);
	int16_t  *buf_2   = aligned_alloc(16,sizeof(int16_t) *BUFFER_SIZE);
	uint8_t  *buf_aux = aligned_alloc(16,sizeof(uint8_t) *BUFFER_SIZE);
	
	//number of byte read
	int nb_block;
	
	//clipping state
	size_t clip[2] = {0, 0};
	
	// conversion function
	void (*conv_function)(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);

#ifdef PERF_MEASURE
	struct timespec start, stop;
	double timeread = 0.0, timeconv = 0.0, timewrite = 0.0;
#endif

	while ((opt = getopt(argc, argv, "i:a:b:x:")) != -1) {
		switch (opt) {
		case 'i':
			input_name_1 = optarg;
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
		default:
			usage();
			break;
		}
	}
	
	if(input_name_1 == NULL || (output_name_1 != NULL && output_name_2 != NULL && output_name_aux != NULL))
	{
		usage();
	}
	
	//reading file 1
	if(input_name_1 != NULL)
	{
		if (strcmp(input_name_1, "-") == 0)// Read samples from stdin
		{
			input_1 = stdin;
		}
		else 
		{
			input_1 = fopen(input_name_1, "rb");
			if (!input_1) {
				fprintf(stderr, "(1) : Failed to open %s\n", input_name_1);
				return -ENOENT;
			}
		}
	}
	
	if(output_name_1 != NULL)
	{
		//opening output file 1
		if (strcmp(output_name_1, "-") == 0)// Read samples from stdin
		{
			output_1 = stdout;
		}
		else
		{
			output_1 = fopen(output_name_1, "wb");
			if (!output_1) {
				fprintf(stderr, "(2) : Failed to open %s\n", output_name_1);
				return -ENOENT;
			}
		}
	}
	
	if(output_name_2 != NULL)
	{
		//opening output file 2
		if (strcmp(output_name_2, "-") == 0)// Read samples from stdin
		{
			output_2 = stdout;
		}
		else if(output_name_2 != NULL)
		{
			output_2 = fopen(output_name_2, "wb");
			if (!output_2) {
				fprintf(stderr, "(2) : Failed to open %s\n", output_name_2);
				return -ENOENT;
			}
		}
	}
	
	if(output_name_aux != NULL)
	{
		//opening output file aux
		if (strcmp(output_name_aux, "-") == 0)// Read samples from stdin
		{
			output_aux = stdout;
		}
		else if(output_name_aux != NULL)
		{
			output_aux = fopen(output_name_aux, "wb");
			if (!output_aux) {
				fprintf(stderr, "(2) : Failed to open %s\n", output_name_aux);
				return -ENOENT;
			}
		}
	}
#if defined(__x86_64__) || defined(_M_X64)
	if(check_cpu_feat()==0) {
		if (output_name_1 == NULL) conv_function = &extract_B_sse;
		else if (output_name_2 == NULL) conv_function = &extract_A_sse;
		else conv_function = &extract_AB_sse;
	}
	else {
#endif
		if (output_name_1 == NULL) conv_function = &extract_B_C;
		else if (output_name_2 == NULL) conv_function = &extract_A_C;
		else conv_function = &extract_AB_C;
#if defined(__x86_64__) || defined(_M_X64)
	}
#endif
	
	if(input_name_1 != NULL && (output_name_1 != NULL || output_name_2 != NULL || output_name_aux != NULL))
	{
		while(!feof(input_1))
		{
#ifdef PERF_MEASURE
			clock_gettime(CLOCK_MONOTONIC, &start);
#endif

			nb_block = fread(buf_tmp,4,BUFFER_SIZE,input_1);

#ifdef PERF_MEASURE
			clock_gettime(CLOCK_MONOTONIC, &stop);
			timeread += (stop.tv_sec - start.tv_sec) * 1e6 + (stop.tv_nsec - start.tv_nsec) / 1e3;
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
#endif

			conv_function(buf_tmp, nb_block, clip, buf_aux, buf_1, buf_2);

#ifdef PERF_MEASURE
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stop);
			timeconv += (stop.tv_sec - start.tv_sec) * 1e6 + (stop.tv_nsec - start.tv_nsec) / 1e3;
#endif

			if(clip[0] > 0)
			{
				fprintf(stderr,"ADC A : %ld samples clipped\n",clip[0]);
				clip[0] = 0; 
			}
			
			if(clip[1] > 0)
			{
				fprintf(stderr,"ADC B : %ld samples clipped\n",clip[1]);
				clip[1] = 0; 
			}
#ifdef PERF_MEASURE
			clock_gettime(CLOCK_MONOTONIC, &start);
#endif
			//write output
			if(output_name_1   != NULL){fwrite(buf_1, 2,nb_block,output_1);}
			if(output_name_2   != NULL){fwrite(buf_2, 2,nb_block,output_2);}
			if(output_name_aux != NULL){fwrite(buf_aux,1,nb_block,output_aux);}

#ifdef PERF_MEASURE
			clock_gettime(CLOCK_MONOTONIC, &stop);
			timewrite += (stop.tv_sec - start.tv_sec) * 1e6 + (stop.tv_nsec - start.tv_nsec) / 1e3;
#endif
			//fflush(output_1);
			//fflush(output_2);
			//fflush(output_aux);
		}
	}

////ending of the program
	
	free(buf_1);
	free(buf_2);
	free(buf_aux);
	
	//Close file 1
	if(input_name_1 != NULL)
	{
		if (input_1 && (input_1 != stdin))
		{
			fclose(input_1);
		}
	}
	
	if(output_name_1 != NULL)
	{
		//Close out file
		if (output_1 && (output_1 != stdout))
		{
			fclose(output_1);
		}
	}
	
	if(output_name_2 != NULL)
	{
		if (output_2 && (output_2 != stdout))
		{
			fclose(output_2);
		}
	}
	
	if(output_name_aux != NULL)
	{
		if (output_aux && (output_aux != stdout))
		{
			fclose(output_aux);
		}
	}

#ifdef PERF_MEASURE
	fprintf(stderr, "Readtime: %f\nConvtime: %f\nwrittime: %f\n", timeread, timeconv, timewrite);
#endif

	return 0;
}
