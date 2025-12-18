
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
	%define level r12
%else
	%define in   rdi
	%define len  rsi
	%define clip rdx
	%define aux  rcx
	%define outA r8
	%define outB r9
	%define level r10
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

%macro STARTP 0
	%if WIN
		mov outA, [rsp+40]
		mov outB, [rsp+48]
		mov rax, [rsp+56]
		sub rsp, 48
		movdqa [rsp+32], xmm6
		movdqa [rsp+16], xmm7
		movdqa [rsp], xmm8
		push level
		mov level, rax
	%else
		mov level, [rsp+8]
	%endif
%endmacro

%macro START 0
	%if WIN
		mov outA, [rsp+40]
		mov outB, [rsp+48]
	%endif
%endmacro

%macro ENDP 0
	%if WIN
		pop level
		movdqa xmm6, [rsp+32]
		movdqa xmm7, [rsp+16]
		movdqa xmm8, [rsp]
		add rsp, 48
	%endif
%endmacro

%macro PEAKCALC16 2
	pshufd xmm0, %2, 0b01001110
	pmaxuw xmm0, %2
	pshuflw %2, xmm0, 0b01001110
	pmaxuw xmm0, %2
	pshuflw %2, xmm0, 0b00000001
	pmaxuw xmm0, %2
	pextrw %1, xmm0, 0
%endmacro

%macro PEAKCALC32 2
	pshufd xmm0, %2, 0b01001110
	pmaxud xmm0, %2
	pshufd %2, xmm0, 0b00000001
	pmaxud xmm0, %2
	pextrw %1, xmm0, 0
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


%macro EXTRACT_AB 0
%if PEAK
	STARTP
	pxor xmm7, xmm7
	pxor xmm8, xmm8
%else
	START
%endif
%%loop_AB:
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
%if PEAK
	pabsw xmm6, xmm5
	pmaxuw xmm7, xmm6
%endif
%if PAD
	psllw xmm5, 4
%endif
	movdqa [outA], xmm5
	movdqa xmm5, [subval]
	psubw xmm5, xmm1
%if PEAK
	pabsw xmm6, xmm5
	pmaxuw xmm8, xmm6
%endif
%if PAD
	psllw xmm5, 4
%endif
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
	jg %%loop_AB
%if PEAK
	PEAKCALC16 rax, xmm7
	PEAKCALC16 rcx, xmm8
	mov [level], ax
	mov [level+2], cx
	ENDP
%endif
	ret
%endmacro

%define PAD 0
%define PEAK 1
global extract_AB_peak_sse
extract_AB_peak_sse:
	EXTRACT_AB

%define PEAK 0
global extract_AB_sse
extract_AB_sse:
	EXTRACT_AB

%define PAD 1
%define PEAK 1
global extract_AB_p_peak_sse
extract_AB_p_peak_sse:
	EXTRACT_AB

%define PEAK 0
global extract_AB_p_sse
extract_AB_p_sse:
	EXTRACT_AB



%macro EXTRACT_AB_32 0
%if PEAK
	STARTP
	pxor xmm7, xmm7
	pxor xmm8, xmm8
%else
	START
%endif
%%loop_AB_32:
	movdqa xmm0, [in]
	movdqa xmm2, xmm0
	pand xmm2, [andmask32]
	movdqa xmm5, [subval32]
	psubd xmm5, xmm2
%if PEAK
	pabsd xmm6, xmm5
	pmaxud xmm7, xmm6
%endif
%if PAD
	pslld xmm5, 4
%endif
	movdqa [outA], xmm5
	movdqa xmm2, xmm0
	psrld xmm2, 20
	movdqa xmm5, [subval32]
	psubd xmm5, xmm2
%if PEAK
	pabsd xmm6, xmm5
	pmaxud xmm8, xmm6
%endif
%if PAD
	pslld xmm5, 4
%endif
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
	jg %%loop_AB_32
%if PEAK
	PEAKCALC32 rax, xmm7
	PEAKCALC32 rcx, xmm8
	mov [level], ax
	mov [level+2], cx
	ENDP
%endif
	ret
%endmacro

%define PAD 0
%define PEAK 1
global extract_AB_peak_32_sse
extract_AB_peak_32_sse:
	EXTRACT_AB_32

%define PEAK 0
global extract_AB_32_sse
extract_AB_32_sse:
	EXTRACT_AB_32

%define PAD 1
%define PEAK 1
global extract_AB_p_peak_32_sse
extract_AB_p_peak_32_sse:
	EXTRACT_AB_32

%define PEAK 0
global extract_AB_p_32_sse
extract_AB_p_32_sse:
	EXTRACT_AB_32



%macro EXTRACT_A_32 0
%if PEAK
	STARTP
	pxor xmm7, xmm7
%else
	START
%endif
%%loop_A_32:
	movdqa xmm0, [in]
	movdqa xmm2, xmm0
	pand xmm2, [andmask32]
	movdqa xmm5, [subval32]
	psubd xmm5, xmm2
%if PEAK
	pabsd xmm6, xmm5
	pmaxud xmm7, xmm6
%endif
%if PAD
	pslld xmm5, 4
%endif
	movdqa [outA], xmm5
	psrld xmm0, 12
	pshufb xmm0, [shuf_aux0]
	movd [aux], xmm0
	movd eax, xmm0
	and eax, [clip_maskA]
	popcnt eax, eax
	add [clip], rax
	movd eax, xmm0
	and eax, [clip_maskB]
	popcnt eax, eax
	add [clip+8], rax
	add aux, 4
	add outA, 16
	add in, 16
	sub len, 4
	jg %%loop_A_32
%if PEAK
	PEAKCALC32 rax, xmm7
	mov [level], ax
	ENDP
%endif
	ret
%endmacro

%define PAD 0

%define PEAK 1
global extract_A_peak_32_sse
extract_A_peak_32_sse:
	EXTRACT_A_32

%define PEAK 0
global extract_A_32_sse
extract_A_32_sse:
	EXTRACT_A_32

%define PAD 1
%define PEAK 1

global extract_A_p_peak_32_sse
extract_A_p_peak_32_sse:
	EXTRACT_A_32

%define PEAK 0
global extract_A_p_32_sse
extract_A_p_32_sse:
	EXTRACT_A_32



%macro EXTRACT_B_32 0
%if PEAK
	STARTP
	pxor xmm7, xmm7
%else
	START
%endif
%%loop_B_32:
	movdqa xmm0, [in]
	movdqa xmm2, xmm0
	psrld xmm2, 20
	movdqa xmm5, [subval32]
	psubd xmm5, xmm2
%if PEAK
	pabsd xmm6, xmm5
	pmaxud xmm7, xmm6
%endif
%if PAD
	pslld xmm5, 4
%endif
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
	add outB, 16
	add in, 16
	sub len, 4
	jg %%loop_B_32
%if PEAK
	PEAKCALC32 rax, xmm7
	mov [level+2], ax
	ENDP
%endif
	ret
%endmacro

%define PAD 0

%define PEAK 1
global extract_B_peak_32_sse
extract_B_peak_32_sse:
	EXTRACT_B_32

%define PEAK 0
global extract_B_32_sse
extract_B_32_sse:
	EXTRACT_B_32

%define PAD 1
%define PEAK 1

global extract_B_p_peak_32_sse
extract_B_p_peak_32_sse:
	EXTRACT_B_32

%define PEAK 0
global extract_B_p_32_sse
extract_B_p_32_sse:
	EXTRACT_B_32



%macro EXTRACT_A 0
%if PEAK
	STARTP
	pxor xmm7, xmm7
%else
	START
%endif
%%loop_A:
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
%if PEAK
	pabsw xmm6, xmm4
	pmaxuw xmm7, xmm6
%endif
%if PAD
	psllw xmm4, 4
%endif
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
	jg %%loop_A
%if PEAK
	PEAKCALC16 rax, xmm7
	mov [level], ax
	ENDP
%endif
	ret
%endmacro

%define PAD 0

%define PEAK 1
global extract_A_peak_sse
extract_A_peak_sse:
	EXTRACT_A

%define PEAK 0
global extract_A_sse
extract_A_sse:
	EXTRACT_A

%define PAD 1
%define PEAK 1

global extract_A_p_peak_sse
extract_A_p_peak_sse:
	EXTRACT_A

%define PEAK 0
global extract_A_p_sse
extract_A_p_sse:
	EXTRACT_A



%macro EXTRACT_B 0
%if PEAK
	STARTP
	pxor xmm7, xmm7
%else
	START
%endif
%%loop_B:
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
%if PEAK
	pabsw xmm6, xmm4
	pmaxuw xmm7, xmm6
%endif
%if PAD
	psllw xmm4, 4
%endif
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
	jg %%loop_B
%if PEAK
	PEAKCALC16 rax, xmm7
	mov [level+2], ax
	ENDP
%endif
	ret
%endmacro

%define PAD 0

%define PEAK 1
global extract_B_peak_sse
extract_B_peak_sse:
	EXTRACT_B

%define PEAK 0
global extract_B_sse
extract_B_sse:
	EXTRACT_B

%define PAD 1
%define PEAK 1

global extract_B_p_peak_sse
extract_B_p_peak_sse:
	EXTRACT_B

%define PEAK 0
global extract_B_p_sse
extract_B_p_sse:
	EXTRACT_B





global extract_S_sse
extract_S_sse:
	START
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
	START
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



; SSE4.1
global convert_16to32_sse
convert_16to32_sse:
	pmovsxwd xmm0, [to32_in]
	movdqa [to32_out], xmm0
	add to32_in, 8
	add to32_out, 16
	sub to32_len, 4
	jg convert_16to32_sse
	ret

; SSE4.1
global convert_16to12to32_sse
convert_16to12to32_sse:
	pmovsxwd xmm0, [to32_in]
	pslld xmm0, 4
	packssdw xmm0, xmm0
	pmovsxwd xmm0, xmm0
	psrad xmm0, 4
	movdqa [to32_out], xmm0
	add to32_in, 8
	add to32_out, 16
	sub to32_len, 4
	jg convert_16to12to32_sse
	ret

; AVX2
global convert_16to32_avx
convert_16to32_avx:
	vpmovsxwd ymm0, [to32_in]
	vmovdqa [to32_out], ymm0
	add to32_in, 16
	add to32_out, 32
	sub to32_len, 8
	jg convert_16to32_avx
	ret

; SSE2
global convert_16to8_sse
convert_16to8_sse:
	movdqa xmm0, [to32_in]
	packsswb xmm0, [to32_in+16]
	movdqa [to32_out], xmm0
	add to32_in, 32
	add to32_out, 16
	sub to32_len, 16
	jg convert_16to8_sse
	ret

; SSE4.1
global convert_16to8to32_sse
convert_16to8to32_sse:
	movdqa xmm0, [to32_in]
	packsswb xmm0, [to32_in+16]
	pmovsxbd xmm1, xmm0
	movdqa [to32_out], xmm1
	psrldq xmm0, 4
	pmovsxbd xmm1, xmm0
	movdqa [to32_out+16], xmm1
	psrldq xmm0, 4
	pmovsxbd xmm1, xmm0
	movdqa [to32_out+32], xmm1
	psrldq xmm0, 4
	pmovsxbd xmm1, xmm0
	movdqa [to32_out+48], xmm1
	add to32_in, 32
	add to32_out, 64
	sub to32_len, 16
	jg convert_16to8to32_sse
	ret

global check_cpu_feat
check_cpu_feat:
	push rbx
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
	pop rbx
	ret
