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

#ifndef EXTRACT_H
#define EXTRACT_H

typedef void (*conv_function_t)(void*,size_t,size_t*,uint8_t*,void*,void*,uint16_t*);
typedef void (*conv_16to32_t)(int16_t*,int32_t*,size_t);
typedef void (*conv_16to8_t)(int16_t*,int8_t*,size_t);

#if defined(__x86_64__) || defined(_M_X64)
void extract_A_sse      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_B_sse      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_AB_sse     (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_S_sse      (uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_A_p_sse    (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_B_p_sse    (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_AB_p_sse   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_S_p_sse    (uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_A_32_sse   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_B_32_sse   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_AB_32_sse  (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_A_p_32_sse (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_B_p_32_sse (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_AB_p_32_sse(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_A_peak_sse      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_B_peak_sse      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_AB_peak_sse     (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_A_p_peak_sse    (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_B_p_peak_sse    (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_AB_p_peak_sse   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_A_peak_32_sse   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_B_peak_32_sse   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_AB_peak_32_sse  (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_A_p_peak_32_sse (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_B_p_peak_32_sse (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_AB_p_peak_32_sse(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);

void convert_16to32_sse (int16_t *in, int32_t *out, size_t len);
void convert_16to32_avx (int16_t *in, int32_t *out, size_t len);
void convert_16to8to32_sse (int16_t *in, int32_t *out, size_t len);
void convert_16to12to32_sse (int16_t *in, int32_t *out, size_t len);
void convert_16to8_sse (int16_t *in, int8_t *out, size_t len);

int check_cpu_feat();
#endif


void extract_X_C      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_XS_C     (uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_A_C      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_B_C      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_AB_C     (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_S_C      (uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_A_p_C    (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_B_p_C    (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_AB_p_C   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_S_p_C    (uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_A_32_C   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_B_32_C   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_AB_32_C  (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_S_32_C   (uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_A_p_32_C (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_B_p_32_C (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_AB_p_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_S_p_32_C (uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_X_peak_C      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_A_peak_C      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_B_peak_C      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_AB_peak_C     (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_A_p_peak_C    (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_B_p_peak_C    (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_AB_p_peak_C   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB, uint16_t *peak_level);
void extract_A_peak_32_C   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_B_peak_32_C   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_AB_peak_32_C  (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_A_p_peak_32_C (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_B_p_peak_32_C (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);
void extract_AB_p_peak_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB, uint16_t *peak_level);

void convert_16to32_C (int16_t *in, int32_t *out, size_t len);
void convert_16to8to32_C (int16_t *in, int32_t *out, size_t len);
void convert_16to12to32_C (int16_t *in, int32_t *out, size_t len);
void convert_16to8_C (int16_t *in, int8_t *out, size_t len);

void extract_audio_2ch_C  (uint16_t *in, size_t len, uint16_t *out12, uint16_t *out34);
void extract_audio_1ch_C  (uint8_t  *in, size_t len, uint8_t   *out1, uint8_t  *out2, uint8_t *out3, uint8_t *out4);

conv_function_t get_conv_function(uint8_t single, uint8_t pad, uint8_t dword, uint8_t peak_level, void* outA, void* outB);
conv_16to32_t get_16to32_function();
conv_16to32_t get_16to8to32_function();
conv_16to32_t get_16to12to32_function();
conv_16to8_t get_16to8_function();

#endif // EXTRACT_H
