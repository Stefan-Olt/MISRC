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

#ifndef EXTRACT_H
#define EXTRACT_H

typedef void (*conv_function_t)(void*,size_t,size_t*,uint8_t*,void*,void*);

#if defined(__x86_64__) || defined(_M_X64)
void extract_A_sse      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_B_sse      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_AB_sse     (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_S_sse      (uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_A_p_sse    (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_B_p_sse    (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_AB_p_sse   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_S_p_sse    (uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_A_32_sse   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);
void extract_B_32_sse   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);
void extract_AB_32_sse  (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);
void extract_A_p_32_sse (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);
void extract_B_p_32_sse (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);
void extract_AB_p_32_sse(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);
int check_cpu_feat();
#endif

void extract_A_C      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_B_C      (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_AB_C     (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_S_C      (uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_A_p_C    (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_B_p_C    (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_AB_p_C   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_S_p_C    (uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int16_t *outA, int16_t *outB);
void extract_A_32_C   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);
void extract_B_32_C   (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);
void extract_AB_32_C  (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);
void extract_S_32_C   (uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);
void extract_A_p_32_C (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);
void extract_B_p_32_C (uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);
void extract_AB_p_32_C(uint32_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);
void extract_S_p_32_C (uint16_t *in, size_t len, size_t *clip, uint8_t *aux, int32_t *outA, int32_t *outB);

conv_function_t get_conv_function(uint8_t single, uint8_t pad, uint8_t dword, void* outA, void* outB);

#endif // EXTRACT_H
