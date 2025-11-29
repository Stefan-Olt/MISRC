/*
* MISRC tools
* Copyright (C) 2024  vrunk11, stefan_o
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
#include <stdint.h>
#include "extract.h"

//bit masking
#define MASK_1      0xFFF
#define MASK_2      0xFFF00000
#define MASK_AUX    0xFF000
#define MASK_AUXS   0xF000

#if defined(__GNUC__)
# define UNUSED(x) x __attribute__((unused))
#else
# define UNUSED(x) x
#endif

void extract_audio_2ch_C(uint16_t *in, size_t len, uint16_t *out12, uint16_t *out34) {
	for(size_t i = 0; i < len/4; i+=3)
	{
		out12[i] = in[i*2];
		out12[i+1] = in[i*2+1];
		out12[i+2] = in[i*2+2];
		out34[i] = in[i*2+3];
		out34[i+1] = in[i*2+4];
		out34[i+2] = in[i*2+5];
	}
}

void extract_audio_1ch_C(uint8_t *in, size_t len, uint8_t *out1, uint8_t *out2, uint8_t *out3, uint8_t *out4) {
	for(size_t i = 0; i < len/4; i+=3)
	{
		out1[i] = in[i*4];
		out1[i+1] = in[i*4+1];
		out1[i+2] = in[i*4+2];
		out2[i] = in[i*4+3];
		out2[i+1] = in[i*4+4];
		out2[i+2] = in[i*4+5];
		out3[i] = in[i*4+6];
		out3[i+1] = in[i*4+7];
		out3[i+2] = in[i*4+8];
		out4[i] = in[i*4+9];
		out4[i+1] = in[i*4+10];
		out4[i+2] = in[i*4+11];
	}
}

void extract_XS_C(uint16_t *in, size_t len, size_t UNUSED(*clip), uint8_t *aux, int16_t UNUSED(*outA), int16_t UNUSED(*outB)) {
	for(size_t i = 0; i < len; i++)
	{
		aux[i] = (in[i] & MASK_AUXS) >> 12;
	}
}

void extract_X_C(uint32_t *in, size_t len, size_t UNUSED(*clip), uint8_t *aux, int16_t UNUSED(*outA), int16_t UNUSED(*outB)) {
	for(size_t i = 0; i < len; i++)
	{
		aux[i] = (in[i] & MASK_AUX) >> 12;
	}
}

void extract_S_C(uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t UNUSED(*outB)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int16_t)(in[i] & MASK_1));
		aux[i]   = (in[i] & MASK_AUXS) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_A_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t UNUSED(*outB)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int16_t)(in[i] & MASK_1));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_B_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t UNUSED(*outA), int16_t *outB) {
	for(size_t i = 0; i < len; i++)
	{
		outB[i]  = 2047 - ((int16_t)((in[i] & MASK_2) >> 20));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_AB_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int16_t)(in[i] & MASK_1));
		outB[i]  = 2047 - ((int16_t)((in[i] & MASK_2) >> 20));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_S_p_C(uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t UNUSED(*outB)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int16_t)(in[i] & MASK_1)))<<4;
		aux[i]   = (in[i] & MASK_AUXS) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_A_p_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t UNUSED(*outB)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int16_t)(in[i] & MASK_1)))<<4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}
void extract_B_p_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t UNUSED(*outA), int16_t *outB) {
	for(size_t i = 0; i < len; i++)
	{
		outB[i]  = (2047 - ((int16_t)((in[i] & MASK_2) >> 20)))<<4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_AB_p_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int16_t)(in[i] & MASK_1)))<<4;
		outB[i]  = (2047 - ((int16_t)((in[i] & MASK_2) >> 20)))<<4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_S_32_C(uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t UNUSED(*outB)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int32_t)(in[i] & MASK_1));
		aux[i]   = (in[i] & MASK_AUXS) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_A_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t UNUSED(*outB)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int32_t)(in[i] & MASK_1));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_B_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t UNUSED(*outA), int32_t *outB) {
	for(size_t i = 0; i < len; i++)
	{
		outB[i]  = 2047 - ((int32_t)((in[i] & MASK_2) >> 20));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_AB_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int32_t)(in[i] & MASK_1));
		outB[i]  = 2047 - ((int32_t)((in[i] & MASK_2) >> 20));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_S_p_32_C(uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t UNUSED(*outB)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int32_t)(in[i] & MASK_1)))<<4;
		aux[i]   = (in[i] & MASK_AUXS) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_A_p_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t UNUSED(*outB)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int32_t)(in[i] & MASK_1)))<<4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}
void extract_B_p_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t UNUSED(*outA), int32_t *outB) {
	for(size_t i = 0; i < len; i++)
	{
		outB[i]  = (2047 - ((int32_t)((in[i] & MASK_2) >> 20)))<<4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_AB_p_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int32_t)(in[i] & MASK_1)))<<4;
		outB[i]  = (2047 - ((int32_t)((in[i] & MASK_2) >> 20)))<<4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void convert_16to32_C(int16_t *in, int32_t *out, size_t len) {
	for(size_t i = 0; i < len; i++)
	{
		out[i] = in[i];
	}
}

conv_function_t get_conv_function(uint8_t single, uint8_t pad, uint8_t dword, void* outA, void* outB) {

	if (outA == NULL && outB == NULL) {
		if (single == 1) return (conv_function_t) &extract_XS_C;
		else return (conv_function_t) &extract_X_C;
	}

#if defined(__x86_64__) || defined(_M_X64)
	if(check_cpu_feat()>=1) {
		fprintf(stderr,"Detected processor with SSSE3 and POPCNT, using optimized extraction routine\n\n");
		if (dword==0) {
			if (pad==1) {
				if (single == 1) return (conv_function_t) &extract_S_p_sse;
				else if (outA == NULL) return (conv_function_t) &extract_B_p_sse;
				else if (outB == NULL) return (conv_function_t) &extract_A_p_sse;
				else return (conv_function_t) &extract_AB_p_sse;
			}
			else {
				if (single == 1) return (conv_function_t) &extract_S_sse;
				else if (outA == NULL) return (conv_function_t) &extract_B_sse;
				else if (outB == NULL) return (conv_function_t) &extract_A_sse;
				else return (conv_function_t) &extract_AB_sse;
			}
		}
		else {
			if (pad==1) {
				if (outA == NULL) return (conv_function_t) &extract_B_p_32_sse;
				else if (outB == NULL) return (conv_function_t) &extract_A_p_32_sse;
				else return (conv_function_t) &extract_AB_p_32_sse;
			}
			else {
				if (outA == NULL) return (conv_function_t) &extract_B_32_sse;
				else if (outB == NULL) return (conv_function_t) &extract_A_32_sse;
				else return (conv_function_t) &extract_AB_32_sse;
			}
			return NULL;
		}
	}
	else {
		fprintf(stderr,"Detected processor without SSSE3 and POPCNT, using standard extraction routine\n\n");
#endif
		if (dword==0) { 
			if (pad==1) {
				if (single == 1) return (conv_function_t) &extract_S_p_C;
				else if (outA == NULL) return (conv_function_t) &extract_B_p_C;
				else if (outB == NULL) return (conv_function_t) &extract_A_p_C;
				else return (conv_function_t) &extract_AB_p_C;
			}
			else {
				if (single == 1) return (conv_function_t) &extract_S_C;
				else if (outA == NULL) return (conv_function_t) &extract_B_C;
				else if (outB == NULL) return (conv_function_t) &extract_A_C;
				else return (conv_function_t) &extract_AB_C;
			}
		}
		else {
			if (pad==1) {
				if (outA == NULL) return (conv_function_t) &extract_B_p_32_C;
				else if (outB == NULL) return (conv_function_t) &extract_A_p_32_C;
				else return (conv_function_t) &extract_AB_p_32_C;
			}
			else {
				if (outA == NULL) return (conv_function_t) &extract_B_32_C;
				else if (outB == NULL) return (conv_function_t) &extract_A_32_C;
				else return (conv_function_t) &extract_AB_32_C;
			}
			return NULL;
		}
#if defined(__x86_64__) || defined(_M_X64)
	}
#endif
}

conv_16to32_t get_16to32_function() {
#if defined(__x86_64__) || defined(_M_X64)
	if(check_cpu_feat()>=2) {
		fprintf(stderr,"Detected processor with SSE4.1, using optimized resampling/repacking routine\n");
		return (conv_16to32_t) &convert_16to32_sse;
	}
	else {
		fprintf(stderr,"Detected processor without SSE4.1, using standard resampling/repacking routine\n");
#endif
		return (conv_16to32_t) &convert_16to32_C;
#if defined(__x86_64__) || defined(_M_X64)
	}
#endif
}
