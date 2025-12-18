/*
* MISRC tools
* Copyright (C) 2024-2025  vrunk11, stefan_o
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

#define INT12_MAX 2047
#define INT12_MIN -2048

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

void extract_XS_C(uint16_t *in, size_t len, size_t UNUSED(*clip), uint8_t *aux, int16_t UNUSED(*outA), int16_t UNUSED(*outB), uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		aux[i] = (in[i] & MASK_AUXS) >> 12;
	}
}

void extract_X_C(uint32_t *in, size_t len, size_t UNUSED(*clip), uint8_t *aux, int16_t UNUSED(*outA), int16_t UNUSED(*outB), uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		aux[i] = (in[i] & MASK_AUX) >> 12;
	}
}

void extract_X_peak_C(uint32_t *in, size_t len, size_t UNUSED(*clip), uint8_t *aux, int16_t UNUSED(*outA), int16_t UNUSED(*outB), uint16_t *peak_level) {
	peak_level[0] = 0;
	peak_level[1] = 0;
	for(size_t i = 0; i < len; i++)
	{
		uint16_t outA = abs(2047 - ((int16_t)(in[i] & MASK_1)));
		uint16_t outB = abs(2047 - ((int16_t)((in[i] & MASK_2) >> 20)));
		aux[i] = (in[i] & MASK_AUX) >> 12;
		if(outA>peak_level[0]) peak_level[0] = outA;
		if(outB>peak_level[1]) peak_level[1] = outB;
	}
}

void extract_S_C(uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t UNUSED(*outB), uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int16_t)(in[i] & MASK_1));
		aux[i]   = (in[i] & MASK_AUXS) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_A_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t UNUSED(*outB), uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int16_t)(in[i] & MASK_1));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_A_peak_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t UNUSED(*outB), uint16_t *peak_level) {
	peak_level[0] = 0;
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int16_t)(in[i] & MASK_1));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		if(abs(outA[i])>peak_level[0]) peak_level[0] = abs(outA[i]);
	}
}

void extract_B_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t UNUSED(*outA), int16_t *outB, uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outB[i]  = 2047 - ((int16_t)((in[i] & MASK_2) >> 20));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_B_peak_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t UNUSED(*outA), int16_t *outB, uint16_t *peak_level) {
	peak_level[1] = 0;
	for(size_t i = 0; i < len; i++)
	{
		outB[i]  = 2047 - ((int16_t)((in[i] & MASK_2) >> 20));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[1] += ((in[i] >> 13) & 1);
		if(abs(outB[i])>peak_level[1]) peak_level[1] = abs(outB[i]);
	}
}

void extract_AB_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int16_t)(in[i] & MASK_1));
		outB[i]  = 2047 - ((int16_t)((in[i] & MASK_2) >> 20));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_AB_peak_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level) {
	peak_level[0] = 0;
	peak_level[1] = 0;
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int16_t)(in[i] & MASK_1));
		outB[i]  = 2047 - ((int16_t)((in[i] & MASK_2) >> 20));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		clip[1] += ((in[i] >> 13) & 1);
		if(abs(outA[i])>peak_level[0]) peak_level[0] = abs(outA[i]);
		if(abs(outB[i])>peak_level[1]) peak_level[1] = abs(outB[i]);
	}
}

void extract_S_p_C(uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t UNUSED(*outB), uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int16_t)(in[i] & MASK_1)))<<4;
		aux[i]   = (in[i] & MASK_AUXS) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_A_p_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t UNUSED(*outB), uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int16_t)(in[i] & MASK_1)))<<4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_A_p_peak_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t UNUSED(*outB), uint16_t *peak_level) {
	peak_level[0] = 0;
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int16_t)(in[i] & MASK_1)));
		if(abs(outA[i])>peak_level[0]) peak_level[0] = abs(outA[i]);
		outA[i]<<= 4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_B_p_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t UNUSED(*outA), int16_t *outB, uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outB[i]  = (2047 - ((int16_t)((in[i] & MASK_2) >> 20)))<<4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_B_p_peak_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t UNUSED(*outA), int16_t *outB, uint16_t *peak_level) {
	peak_level[1] = 0;
	for(size_t i = 0; i < len; i++)
	{
		outB[i]  = (2047 - ((int16_t)((in[i] & MASK_2) >> 20)));
		if(abs(outB[i])>peak_level[1]) peak_level[1] = abs(outB[i]);
		outB[i]<<= 4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_AB_p_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int16_t)(in[i] & MASK_1)))<<4;
		outB[i]  = (2047 - ((int16_t)((in[i] & MASK_2) >> 20)))<<4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_AB_p_peak_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level) {
	peak_level[0] = 0;
	peak_level[1] = 0;
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int16_t)(in[i] & MASK_1)));
		if(abs(outA[i])>peak_level[0]) peak_level[0] = abs(outA[i]);
		outA[i]<<= 4;
		outB[i]  = (2047 - ((int16_t)((in[i] & MASK_2) >> 20)));
		if(abs(outB[i])>peak_level[1]) peak_level[1] = abs(outB[i]);
		outB[i]<<= 4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_S_32_C(uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t UNUSED(*outB), uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int32_t)(in[i] & MASK_1));
		aux[i]   = (in[i] & MASK_AUXS) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_A_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t UNUSED(*outB), uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int32_t)(in[i] & MASK_1));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_A_peak_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t UNUSED(*outB), uint16_t *peak_level) {
	peak_level[0] = 0;
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int16_t)(in[i] & MASK_1));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		if(abs(outA[i])>peak_level[0]) peak_level[0] = abs(outA[i]);
	}
}


void extract_B_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t UNUSED(*outA), int32_t *outB, uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outB[i]  = 2047 - ((int32_t)((in[i] & MASK_2) >> 20));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_B_peak_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t UNUSED(*outA), int32_t *outB, uint16_t *peak_level) {
	peak_level[1] = 0;
	for(size_t i = 0; i < len; i++)
	{
		outB[i]  = 2047 - ((int16_t)((in[i] & MASK_2) >> 20));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[1] += ((in[i] >> 13) & 1);
		if(abs(outB[i])>peak_level[1]) peak_level[1] = abs(outB[i]);
	}
}

void extract_AB_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int32_t)(in[i] & MASK_1));
		outB[i]  = 2047 - ((int32_t)((in[i] & MASK_2) >> 20));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_AB_peak_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level) {
	peak_level[0] = 0;
	peak_level[1] = 0;
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = 2047 - ((int16_t)(in[i] & MASK_1));
		outB[i]  = 2047 - ((int16_t)((in[i] & MASK_2) >> 20));
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		clip[1] += ((in[i] >> 13) & 1);
		if(abs(outA[i])>peak_level[0]) peak_level[0] = abs(outA[i]);
		if(abs(outB[i])>peak_level[1]) peak_level[1] = abs(outB[i]);
	}
}

void extract_S_p_32_C(uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t UNUSED(*outB), uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int32_t)(in[i] & MASK_1)))<<4;
		aux[i]   = (in[i] & MASK_AUXS) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_A_p_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t UNUSED(*outB), uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int32_t)(in[i] & MASK_1)))<<4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_A_p_peak_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t UNUSED(*outB), uint16_t *peak_level) {
	peak_level[0] = 0;
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int16_t)(in[i] & MASK_1)));
		if(abs(outA[i])>peak_level[0]) peak_level[0] = abs(outA[i]);
		outA[i]<<= 4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
	}
}

void extract_B_p_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t UNUSED(*outA), int32_t *outB, uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outB[i]  = (2047 - ((int32_t)((in[i] & MASK_2) >> 20)))<<4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_B_p_peak_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t UNUSED(*outA), int32_t *outB, uint16_t *peak_level) {
	peak_level[1] = 0;
	for(size_t i = 0; i < len; i++)
	{
		outB[i]  = (2047 - ((int16_t)((in[i] & MASK_2) >> 20)));
		if(abs(outB[i])>peak_level[1]) peak_level[1] = abs(outB[i]);
		outB[i]<<= 4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_AB_p_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t UNUSED(*peak_level)) {
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int32_t)(in[i] & MASK_1)))<<4;
		outB[i]  = (2047 - ((int32_t)((in[i] & MASK_2) >> 20)))<<4;
		aux[i]   = (in[i] & MASK_AUX) >> 12;
		clip[0] += ((in[i] >> 12) & 1);
		clip[1] += ((in[i] >> 13) & 1);
	}
}

void extract_AB_p_peak_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level) {
	peak_level[0] = 0;
	peak_level[1] = 0;
	for(size_t i = 0; i < len; i++)
	{
		outA[i]  = (2047 - ((int16_t)(in[i] & MASK_1)));
		if(abs(outA[i])>peak_level[0]) peak_level[0] = abs(outA[i]);
		outA[i]<<= 4;
		outB[i]  = (2047 - ((int16_t)((in[i] & MASK_2) >> 20)));
		if(abs(outB[i])>peak_level[1]) peak_level[1] = abs(outB[i]);
		outB[i]<<= 4;
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

void convert_16to8to32_C(int16_t *in, int32_t *out, size_t len) {
	for(size_t i = 0; i < len; i++)
	{
		out[i] = (in[i]>INT8_MAX) ? INT8_MAX : ((in[i]<INT8_MIN) ? INT8_MIN : in[i]);
	}
}

void convert_16to12to32_C(int16_t *in, int32_t *out, size_t len) {
	for(size_t i = 0; i < len; i++)
	{
		out[i] = (in[i]>INT12_MAX) ? INT12_MAX : ((in[i]<INT12_MIN) ? INT12_MIN : in[i]);
	}
}

void convert_16to8_C(int16_t *in, int8_t *out, size_t len) {
	for(size_t i = 0; i < len; i++)
	{
		out[i] = (in[i]>INT8_MAX) ? INT8_MAX : ((in[i]<INT8_MIN) ? INT8_MIN : in[i]);
	}
}

/* untested, therefore not used yet */
#if defined(__aarch64__) || defined(__arm64__)
#include <arm_neon.h>

void convert_16to8_arm(int16_t *in, int8_t *out, size_t len)
{
	int16_t *src = in;
	int8_t  *dst = out;
#ifdef __builtin_assume_aligned
	src = (int16_t *)__builtin_assume_aligned(src, 16);
	dst = (int8_t  *)__builtin_assume_aligned(dst, 16);
#endif
	for (size_t i = 0; i < len; i += 16) {
		int16x8_t a = vld1q_s16(src);       // load 8 x int16
		int16x8_t b = vld1q_s16(src + 8);   // load next 8 x int16
		int8x8_t   lo = vqmovn_s16(a);          // SQXTN: saturate+narrow low 8 -> int8x8_t
		int8x16_t  v  = vqmovn_high_s16(lo, b); // SQXTN2: saturate+narrow high 8 into top half
		vst1q_s8(dst, v);                   // store 16 x int8
		src += 16;
		dst += 16;
	}
}

void convert_16to8to32_arm(int16_t *in, int32_t *out, size_t len)
{
	int16_t *src = in;
	int32_t *dst = out;
#ifdef __builtin_assume_aligned
	src = (int16_t *)__builtin_assume_aligned(src, 16);
	dst = (int8_t  *)__builtin_assume_aligned(dst, 16);
#endif
	for (size_t i = 0; i < len; i += 16) {
		// Load 16 x int16_t as two 8-lane vectors
		int16x8_t a = vld1q_s16(src);
		int16x8_t b = vld1q_s16(src + 8);
		// Saturating narrow to signed 8-bit ([-128, 127])
		int8x8_t a8 = vqmovn_s16(a);
		int8x8_t b8 = vqmovn_s16(b);
		// Widen to signed 16-bit
		int16x8_t a16 = vmovl_s8(a8);
		int16x8_t b16 = vmovl_s8(b8);
		// Widen to signed 32-bit (two 4-lane groups per 8-lane vector)
		int32x4_t a32_lo = vmovl_s16(vget_low_s16(a16));
		int32x4_t a32_hi = vmovl_s16(vget_high_s16(a16));
		int32x4_t b32_lo = vmovl_s16(vget_low_s16(b16));
		int32x4_t b32_hi = vmovl_s16(vget_high_s16(b16));
		// Store 16 x int32_t
		vst1q_s32(dst + 0,  a32_lo);
		vst1q_s32(dst + 4,  a32_hi);
		vst1q_s32(dst + 8,  b32_lo);
		vst1q_s32(dst + 12, b32_hi);
		src += 16;
		dst += 16;
	}
}
#endif

conv_function_t get_conv_function(uint8_t single, uint8_t pad, uint8_t dword, uint8_t peak_level, void* outA, void* outB) {

	if (single == 1) peak_level = 0;

	if (outA == NULL && outB == NULL) {
		if (single == 1) return (conv_function_t) &extract_XS_C;
		if (peak_level) return (conv_function_t) &extract_X_peak_C;
		else return (conv_function_t) &extract_X_C;
	}
#if defined(__x86_64__) || defined(_M_X64)
	if (peak_level == 1) {
		if(check_cpu_feat()>=2) {
			fprintf(stderr,"Detected processor with SSE4.1, using optimized extraction routine\n\n");
			if (dword==0) {
				if (pad==1) {
					if (outA == NULL) return (conv_function_t) &extract_B_p_peak_sse;
					if (outB == NULL) return (conv_function_t) &extract_A_p_peak_sse;
					return (conv_function_t) &extract_AB_p_peak_sse;
				}
				else {
					if (outA == NULL) return (conv_function_t) &extract_B_peak_sse;
					if (outB == NULL) return (conv_function_t) &extract_A_peak_sse;
					return (conv_function_t) &extract_AB_peak_sse;
				}
			}
			else {
				if (pad==1) {
					if (outA == NULL) return (conv_function_t) &extract_B_p_peak_32_sse;
					if (outB == NULL) return (conv_function_t) &extract_A_p_peak_32_sse;
					return (conv_function_t) &extract_AB_p_peak_32_sse;
				}
				else {
					if (outA == NULL) return (conv_function_t) &extract_B_peak_32_sse;
					if (outB == NULL) return (conv_function_t) &extract_A_peak_32_sse;
					return (conv_function_t) &extract_AB_peak_32_sse;
				}
			}
		}
	}
	else {
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
			}
		}
	}
	if (peak_level == 1) fprintf(stderr,"Detected processor without SSE4.1, using standard extraction routine\n\n");
	else  fprintf(stderr,"Detected processor without SSSE3 and POPCNT, using standard extraction routine\n\n");
#endif
	if (peak_level == 1) {
		if (dword==0) { 
			if (pad==1) {
				if (outA == NULL) return (conv_function_t) &extract_B_p_peak_C;
				if (outB == NULL) return (conv_function_t) &extract_A_p_peak_C;
				return (conv_function_t) &extract_AB_p_peak_C;
			}
			else {
				if (outA == NULL) return (conv_function_t) &extract_B_peak_C;
				if (outB == NULL) return (conv_function_t) &extract_A_peak_C;
				return (conv_function_t) &extract_AB_peak_C;
			}
		}
		else {
			if (pad==1) {
				if (outA == NULL) return (conv_function_t) &extract_B_p_peak_32_C;
				if (outB == NULL) return (conv_function_t) &extract_A_p_peak_32_C;
				return (conv_function_t) &extract_AB_p_peak_32_C;
			}
			else {
				if (outA == NULL) return (conv_function_t) &extract_B_peak_32_C;
				if (outB == NULL) return (conv_function_t) &extract_A_peak_32_C;
				return (conv_function_t) &extract_AB_peak_32_C;
			}
		}
	}
	else {
		if (dword==0) { 
			if (pad==1) {
				if (single == 1) return (conv_function_t) &extract_S_p_C;
				if (outA == NULL) return (conv_function_t) &extract_B_p_C;
				if (outB == NULL) return (conv_function_t) &extract_A_p_C;
				return (conv_function_t) &extract_AB_p_C;
			}
			else {
				if (single == 1) return (conv_function_t) &extract_S_C;
				if (outA == NULL) return (conv_function_t) &extract_B_C;
				if (outB == NULL) return (conv_function_t) &extract_A_C;
				return (conv_function_t) &extract_AB_C;
			}
		}
		else {
			if (pad==1) {
				if (outA == NULL) return (conv_function_t) &extract_B_p_32_C;
				if (outB == NULL) return (conv_function_t) &extract_A_p_32_C;
				return (conv_function_t) &extract_AB_p_32_C;
			}
			else {
				if (outA == NULL) return (conv_function_t) &extract_B_32_C;
				if (outB == NULL) return (conv_function_t) &extract_A_32_C;
				return (conv_function_t) &extract_AB_32_C;
			}
		}
	}
	return NULL;
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

conv_16to32_t get_16to8to32_function() {
#if defined(__x86_64__) || defined(_M_X64)
	if(check_cpu_feat()>=2) {
		fprintf(stderr,"Detected processor with SSE4.1, using optimized 8 bit repacking routine\n");
		return (conv_16to32_t) &convert_16to8to32_sse;
	}
	else {
		fprintf(stderr,"Detected processor without SSE4.1, using standard 8 bit repacking routine\n");
#endif
		return (conv_16to32_t) &convert_16to8to32_C;
#if defined(__x86_64__) || defined(_M_X64)
	}
#endif
}

conv_16to32_t get_16to12to32_function() {
#if defined(__x86_64__) || defined(_M_X64)
	if(check_cpu_feat()>=2) {
		fprintf(stderr,"Detected processor with SSE4.1, using optimized 8 bit repacking routine\n");
		return (conv_16to32_t) &convert_16to12to32_sse;
	}
	else {
		fprintf(stderr,"Detected processor without SSE4.1, using standard 8 bit repacking routine\n");
#endif
		return (conv_16to32_t) &convert_16to12to32_C;
#if defined(__x86_64__) || defined(_M_X64)
	}
#endif
}

conv_16to8_t get_16to8_function() {
#if defined(__x86_64__) || defined(_M_X64) // SSE2 is mandatory on x86_64
	return (conv_16to8_t) &convert_16to8_sse;
#else
	return (conv_16to8_t) &convert_16to8_C;
#endif
}
