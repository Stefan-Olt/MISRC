
; MISRC tools
; Copyright (C) 2024  vrunk11, stefan_o
;
; This program is free software: you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 3 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program.  If not, see <http://www.gnu.org/licenses/>.


%define STACK_ALIGNMENT 16
default rel

%define WIN  0

%ifidn __OUTPUT_FORMAT__,win32
	%define WIN  1
%elifidn __OUTPUT_FORMAT__,win64
	%define WIN  1
%elifidn __OUTPUT_FORMAT__,x64
	%define WIN  1
%endif

%if WIN
	%define in   rcx
	%define len  rdx
	%define clip r8
	%define aux  r9
	%define outA r10
	%define outB r11
%else
	%define in   rdi
	%define len  rsi
	%define clip rdx
	%define aux  rcx
	%define outA r8
	%define outB r9
%endif

%if WIN
	%define to32_in   rcx
	%define to32_out  rdx
	%define to32_len  r8
%else
	%define to32_in   rdi
	%define to32_out  rsi
	%define to32_len  rdx
%endif

%macro POPARGS 0
	%if WIN
		mov outA, [rsp+40]
		mov outB, [rsp+48]
	%endif
%endmacro

%ifidn __OUTPUT_FORMAT__,elf64
	section .note.GNU-stack noalloc noexec nowrite progbits
%endif

section .data
	ALIGN 16
	shuf_dat:   db	 0,	1,	4,	5,	8,	9,   12,   13,	2,	3,	6,	7,  10,	11,   14,   15
	shuf_aux0:  db	 0,	4,	8,   12, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
	shuf_aux1:  db  0x80, 0x80, 0x80, 0x80,	0,	4,	8,   12, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
	shuf_auxS:  db	 0,	2,	4,	6,	8,   10,   12,   14, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
	andmask:	db  0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f, 0xff, 0x0f
	andmask32:  db  0xff, 0x0f, 0x00, 0x00, 0xff, 0x0f, 0x00, 0x00, 0xff, 0x0f, 0x00, 0x00, 0xff, 0x0f, 0x00, 0x00
	subval:	 dw  2047, 2047, 2047, 2047, 2047, 2047, 2047, 2047
	subval32:   dd  2047, 2047, 2047, 2047
	clip_maskA: db	 1,	1,	1,	1,	1,	1,	1,	1
	clip_maskB: db	 2,	2,	2,	2,	2,	2,	2,	2

section .text

global extract_AB_sse
extract_AB_sse:
	POPARGS
loop_AB_0:
	movdqa xmm0, [in]
	movdqa xmm1, [in+16]
	movdqa xmm2, xmm0
	movdqa xmm3, xmm1
	pshufb xmm0, [shuf_dat]
	pshufb xmm1, [shuf_dat]
	movdqa xmm4, xmm0
	movlhps xmm4, xmm1
	movhlps xmm1, xmm0
	psrlw xmm1, 4
	pand xmm4, [andmask]
	movdqa xmm5, [subval]
	psubw xmm5, xmm4
	movdqa [outA], xmm5
	movdqa xmm5, [subval]
	psubw xmm5, xmm1
	movdqa [outB], xmm5
	psrld xmm2, 12
	psrld xmm3, 12
	pshufb xmm2, [shuf_aux0]
	pshufb xmm3, [shuf_aux1]
	por xmm2, xmm3
	movlpd [aux], xmm2
	movq rax, xmm2
	and rax, [clip_maskA]
	popcnt rax, rax
	add [clip], rax
	movq rax, xmm2
	and rax, [clip_maskB]
	popcnt rax, rax
	add [clip+8], rax
	add aux, 8
	add outA, 16
	add outB, 16
	add in, 32
	sub len, 8
	jg loop_AB_0
	ret

global extract_AB_32_sse
extract_AB_32_sse:
	POPARGS
loop_AB_32_0:
	movdqa xmm0, [in]
	movdqa xmm2, xmm0
	pand xmm2, [andmask32]
	movdqa xmm5, [subval32]
	psubd xmm5, xmm2
	movdqa [outA], xmm5
	movdqa xmm2, xmm0
	psrld xmm2, 20
	movdqa xmm5, [subval32]
	psubd xmm5, xmm2
	movdqa [outB], xmm5
	psrld xmm0, 12
	pshufb xmm0, [shuf_aux0]
	movd [aux], xmm0
	movd eax, xmm0
	and eax, [clip_maskA]
	popcnt eax, eax
	add [clip], eax
	movd eax, xmm0
	and eax, [clip_maskB]
	popcnt eax, eax
	add [clip+8], eax
	add aux, 4
	add outA, 16
	add outB, 16
	add in, 16
	sub len, 4
	jg loop_AB_32_0
	ret

global extract_AB_p_32_sse
extract_AB_p_32_sse:
	POPARGS
loop_AB_p_32_0:
	movdqa xmm0, [in]
	movdqa xmm2, xmm0
	pand xmm2, [andmask32]
	movdqa xmm5, [subval32]
	psubd xmm5, xmm2
	pslld xmm5, 4
	movdqa [outA], xmm5
	movdqa xmm2, xmm0
	psrld xmm2, 20
	movdqa xmm5, [subval32]
	psubd xmm5, xmm2
	pslld xmm5, 4
	movdqa [outB], xmm5
	psrld xmm0, 12
	pshufb xmm0, [shuf_aux0]
	movd [aux], xmm0
	movd eax, xmm0
	and eax, [clip_maskA]
	popcnt eax, eax
	add [clip], eax
	movd eax, xmm0
	and eax, [clip_maskB]
	popcnt eax, eax
	add [clip+8], eax
	add aux, 4
	add outA, 16
	add outB, 16
	add in, 16
	sub len, 4
	jg loop_AB_p_32_0
	ret

global extract_A_32_sse
extract_A_32_sse:
	POPARGS
loop_A_32_0:
	movdqa xmm0, [in]
	movdqa xmm2, xmm0
	pand xmm2, [andmask32]
	movdqa xmm5, [subval32]
	psubd xmm5, xmm2
	movdqa [outA], xmm5
	psrld xmm0, 12
	pshufb xmm0, [shuf_aux0]
	movd [aux], xmm0
	movd eax, xmm0
	and eax, [clip_maskA]
	popcnt eax, eax
	add [clip], eax
	movd eax, xmm0
	and eax, [clip_maskB]
	popcnt eax, eax
	add [clip+8], eax
	add aux, 4
	add outA, 16
	add in, 16
	sub len, 4
	jg loop_A_32_0
	ret

global extract_A_p_32_sse
extract_A_p_32_sse:
	POPARGS
loop_A_p_32_0:
	movdqa xmm0, [in]
	movdqa xmm2, xmm0
	pand xmm2, [andmask32]
	movdqa xmm5, [subval32]
	psubd xmm5, xmm2
	pslld xmm5, 4
	movdqa [outA], xmm5
	psrld xmm0, 12
	pshufb xmm0, [shuf_aux0]
	movd [aux], xmm0
	movd eax, xmm0
	and eax, [clip_maskA]
	popcnt eax, eax
	add [clip], eax
	movd eax, xmm0
	and eax, [clip_maskB]
	popcnt eax, eax
	add [clip+8], eax
	add aux, 4
	add outA, 16
	add in, 16
	sub len, 4
	jg loop_A_p_32_0
	ret

global extract_B_32_sse
extract_B_32_sse:
	POPARGS
loop_B_32_0:
	movdqa xmm0, [in]
	movdqa xmm2, xmm0
	psrld xmm2, 20
	movdqa xmm5, [subval32]
	psubd xmm5, xmm2
	movdqa [outB], xmm5
	psrld xmm0, 12
	pshufb xmm0, [shuf_aux0]
	movd [aux], xmm0
	movd eax, xmm0
	and eax, [clip_maskA]
	popcnt eax, eax
	add [clip], eax
	movd eax, xmm0
	and eax, [clip_maskB]
	popcnt eax, eax
	add [clip+8], eax
	add aux, 4
	add outA, 16
	add outB, 16
	add in, 16
	sub len, 4
	jg loop_B_32_0
	ret

global extract_B_p_32_sse
extract_B_p_32_sse:
	POPARGS
	movdqa xmm5, [subval32]
loop_B_p_32_0:
	movdqa xmm0, [in]
	movdqa xmm2, xmm0
	psrld xmm2, 20
	movdqa xmm5, [subval32]
	psubd xmm5, xmm2
	pslld xmm5, 4
	movdqa [outB], xmm5
	psrld xmm0, 12
	pshufb xmm0, [shuf_aux0]
	movd [aux], xmm0
	movd eax, xmm0
	and eax, [clip_maskA]
	popcnt eax, eax
	add [clip], eax
	movd eax, xmm0
	and eax, [clip_maskB]
	popcnt eax, eax
	add [clip+8], eax
	add aux, 4
	add outA, 16
	add outB, 16
	add in, 16
	sub len, 4
	jg loop_B_p_32_0
	ret

global extract_A_sse
extract_A_sse:
	POPARGS
loop_A_0:
	movdqa xmm0, [in]
	movdqa xmm1, [in+16]
	movdqa xmm2, xmm0
	movdqa xmm3, xmm1
	pshufb xmm0, [shuf_dat]
	pshufb xmm1, [shuf_dat]
	movlhps xmm0, xmm1
	pand xmm0, [andmask]
	movdqa xmm4, [subval]
	psubw xmm4, xmm0
	movdqa [outA], xmm4
	psrld xmm2, 12
	psrld xmm3, 12
	pshufb xmm2, [shuf_aux0]
	pshufb xmm3, [shuf_aux1]
	por xmm2, xmm3
	movlpd [aux], xmm2
	movq rax, xmm2
	and rax, [clip_maskA]
	popcnt rax, rax
	add [clip], rax
	add aux, 8
	add outA, 16
	add in, 32
	sub len, 8
	jg loop_A_0
	ret

global extract_S_sse
extract_S_sse:
	POPARGS
loop_S_0:
	movdqa xmm0, [in]
	movdqa xmm2, xmm0
	pand xmm0, [andmask]
	movdqa xmm4, [subval]
	psubw xmm4, xmm0
	movdqa [outA], xmm4
	psrlw xmm2, 12
	pshufb xmm2, [shuf_auxS]
	movlpd [aux], xmm2
	movq rax, xmm2
	and rax, [clip_maskA]
	popcnt rax, rax
	add [clip], rax
	add aux, 8
	add outA, 16
	add in, 16
	sub len, 8
	jg loop_S_0
	ret

global extract_S_p_sse
extract_S_p_sse:
	POPARGS
loop_S_p_0:
	movdqa xmm0, [in]
	movdqa xmm2, xmm0
	pand xmm0, [andmask]
	movdqa xmm4, [subval]
	psubw xmm4, xmm0
	psllw xmm4, 4
	movdqa [outA], xmm4
	psrlw xmm2, 12
	pshufb xmm2, [shuf_auxS]
	movlpd [aux], xmm2
	movq rax, xmm2
	and rax, [clip_maskA]
	popcnt rax, rax
	add [clip], rax
	add aux, 8
	add outA, 16
	add in, 16
	sub len, 8
	jg loop_S_p_0
	ret

global extract_B_sse
extract_B_sse:
	POPARGS
loop_B_0:
	movdqa xmm0, [in]
	movdqa xmm1, [in+16]
	movdqa xmm2, xmm0
	movdqa xmm3, xmm1
	pshufb xmm0, [shuf_dat]
	pshufb xmm1, [shuf_dat]
	movhlps xmm1, xmm0
	psrlw xmm1, 4
	movdqa xmm4, [subval]
	psubw xmm4, xmm1
	movdqa [outB], xmm4
	psrld xmm2, 12
	psrld xmm3, 12
	pshufb xmm2, [shuf_aux0]
	pshufb xmm3, [shuf_aux1]
	por xmm2, xmm3
	movlpd [aux], xmm2
	movq rax, xmm2
	and rax, [clip_maskB]
	popcnt rax, rax
	add [clip+8], rax
	add aux, 8
	add outB, 16
	add in, 32
	sub len, 8
	jg loop_B_0
	ret

global extract_AB_p_sse
extract_AB_p_sse:
	POPARGS
loop_AB_p_0:
	movdqa xmm0, [in]
	movdqa xmm1, [in+16]
	movdqa xmm2, xmm0
	movdqa xmm3, xmm1
	pshufb xmm0, [shuf_dat]
	pshufb xmm1, [shuf_dat]
	movdqa xmm4, xmm0
	movlhps xmm4, xmm1
	movhlps xmm1, xmm0
	psrlw xmm1, 4
	pand xmm4, [andmask]
	movdqa xmm5, [subval]
	psubw xmm5, xmm4
	psllw xmm5, 4
	movdqa [outA], xmm5
	movdqa xmm5, [subval]
	psubw xmm5, xmm1
	psllw xmm5, 4
	movdqa [outB], xmm5
	psrld xmm2, 12
	psrld xmm3, 12
	pshufb xmm2, [shuf_aux0]
	pshufb xmm3, [shuf_aux1]
	por xmm2, xmm3
	movlpd [aux], xmm2
	movq rax, xmm2
	and rax, [clip_maskA]
	popcnt rax, rax
	add [clip], rax
	movq rax, xmm2
	and rax, [clip_maskB]
	popcnt rax, rax
	add [clip+8], rax
	add aux, 8
	add outA, 16
	add outB, 16
	add in, 32
	sub len, 8
	jg loop_AB_p_0
	ret

global extract_A_p_sse
extract_A_p_sse:
	POPARGS
loop_A_p_0:
	movdqa xmm0, [in]
	movdqa xmm1, [in+16]
	movdqa xmm2, xmm0
	movdqa xmm3, xmm1
	pshufb xmm0, [shuf_dat]
	pshufb xmm1, [shuf_dat]
	movlhps xmm0, xmm1
	pand xmm0, [andmask]
	movdqa xmm4, [subval]
	psubw xmm4, xmm0
	psllw xmm4, 4
	movdqa [outA], xmm4
	psrld xmm2, 12
	psrld xmm3, 12
	pshufb xmm2, [shuf_aux0]
	pshufb xmm3, [shuf_aux1]
	por xmm2, xmm3
	movlpd [aux], xmm2
	movq rax, xmm2
	and rax, [clip_maskA]
	popcnt rax, rax
	add [clip], rax
	add aux, 8
	add outA, 16
	add in, 32
	sub len, 8
	jg loop_A_p_0
	ret

global extract_B_p_sse
extract_B_p_sse:
	POPARGS
loop_B_p_0:
	movdqa xmm0, [in]
	movdqa xmm1, [in+16]
	movdqa xmm2, xmm0
	movdqa xmm3, xmm1
	pshufb xmm0, [shuf_dat]
	pshufb xmm1, [shuf_dat]
	movhlps xmm1, xmm0
	psrlw xmm1, 4
	movdqa xmm4, [subval]
	psubw xmm4, xmm1
	psllw xmm4, 4
	movdqa [outB], xmm4
	psrld xmm2, 12
	psrld xmm3, 12
	pshufb xmm2, [shuf_aux0]
	pshufb xmm3, [shuf_aux1]
	por xmm2, xmm3
	movlpd [aux], xmm2
	movq rax, xmm2
	and rax, [clip_maskB]
	popcnt rax, rax
	add [clip+8], rax
	add aux, 8
	add outB, 16
	add in, 32
	sub len, 8
	jg loop_B_p_0
	ret

global convert_16to32_sse
convert_16to32_sse:
	pmovsxwd xmm0, [to32_in]
	movdqa [to32_out], xmm0
	add to32_in, 8
	add to32_out, 16
	sub to32_len, 4
	jg convert_16to32_sse
	ret

global convert_16to32_avx
convert_16to32_avx:
	vpmovsxwd ymm0, [to32_in]
	vmovdqa [to32_out], ymm0
	add to32_in, 16
	add to32_out, 32
	sub to32_len, 8
	jg convert_16to32_avx
	ret

global check_cpu_feat
check_cpu_feat:
	mov eax, 1
	cpuid
	xor eax, eax
	mov edx, ecx
	and edx, 0x00800201 ; check for SSE3, SSSE3 and popcnt
	sub edx, 0x00800201
	jnz check_cpu_feat_end
	inc eax
	and ecx, 0x02000000 ; check for SSE4.1
	jz check_cpu_feat_end
	inc eax
check_cpu_feat_end:
	ret
