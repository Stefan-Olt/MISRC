/*
* extract_test
* Copyright (C) 2024  vrunk11, stefan_o
* 
* This program will test the extraction functions
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "extract.h"

#define BUFSIZE ((2<<16)*1024)

typedef struct {
	conv_function_t C;
	conv_function_t S;
	size_t a_cmp;
	size_t b_cmp;
} conv_test_t;

int main() {
	FILE *rnd;
	void *buf;
	void *bufAa, *bufAb;
	void *bufBa, *bufBb;
	void *bufAUXa, *bufAUXb;
	size_t clipa[2], clipb[2];

	conv_test_t cvs[] = {
		/* 0*/ {extract_A_C, extract_A_sse, BUFSIZE>>1, 0 },
		/* 1*/ {extract_B_C, extract_B_sse, 0, BUFSIZE>>1 },
		/* 2*/ {extract_AB_C, extract_AB_sse, BUFSIZE>>1, BUFSIZE>>1 },
		/* 3*/ {extract_S_C, extract_S_sse, BUFSIZE>>2, 0},
		/* 4*/ {extract_A_p_C, extract_A_p_sse, BUFSIZE>>1, 0 },
		/* 5*/ {extract_B_p_C, extract_B_p_sse, 0, BUFSIZE>>1 },
		/* 6*/ {extract_AB_p_C, extract_AB_p_sse, BUFSIZE>>1, BUFSIZE>>1 },
		/* 7*/ {extract_S_p_C, extract_S_p_sse, BUFSIZE>>2, 0 },
		/* 8*/ {extract_A_32_C, extract_A_32_sse, BUFSIZE, 0 },
		/* 9*/ {extract_B_32_C, extract_B_32_sse, 0, BUFSIZE },
		/*10*/ {extract_AB_32_C, extract_AB_32_sse, BUFSIZE, BUFSIZE },
		/*11*/ {extract_A_p_32_C, extract_A_p_32_sse, BUFSIZE, 0 },
		/*12*/ {extract_B_p_32_C, extract_B_p_32_sse, 0, BUFSIZE },
		/*13*/ {extract_AB_p_32_C, extract_AB_p_32_sse, BUFSIZE, BUFSIZE}
	};

	rnd = fopen("/dev/urandom","rb");
	buf = malloc(BUFSIZE);
	bufAa = malloc(BUFSIZE);
	bufAb = malloc(BUFSIZE);
	bufBa = malloc(BUFSIZE);
	bufBb = malloc(BUFSIZE);
	bufAUXa = malloc(BUFSIZE);
	bufAUXb = malloc(BUFSIZE);
	fread(buf,1,BUFSIZE,rnd);
	fclose(rnd);

	fprintf(stderr,"Testing C and ASM extraction functions by comparison with random data.\n");

	for(int i=0; i<sizeof(cvs)/sizeof(cvs[0]);i++) {
		fprintf(stderr,"Testing %i...\n", i);
		clipa[0] = 0;
		clipa[1] = 0;
		clipb[0] = 0;
		clipb[1] = 0;
		cvs[i].C(buf,BUFSIZE>>2,clipa,bufAUXa,bufAa,bufBa);
		cvs[i].S(buf,BUFSIZE>>2,clipb,bufAUXb,bufAb,bufBb);
		if(cvs[i].a_cmp > 0) {
			if(clipa[0] != clipb[0]) fprintf(stderr, "%i Incorrect Clip A: %ul vs %ul\n", i, clipa[0], clipb[0]);
			if(memcmp(bufAa,bufAb,cvs[i].a_cmp)!=0) {
				fprintf(stderr, "%i Incorrect Buffer A\n", i);
				for(int j=0;j<16;j++) {
					fprintf(stderr, "%08x %08x %08x\n", *(((uint32_t*)(buf))+j),*(((uint32_t*)(bufAa))+j),*(((uint32_t*)(bufAb))+j));
				}
			}
		}
		if(cvs[i].b_cmp > 0) {
			if(clipa[1] != clipb[1]) fprintf(stderr, "%i Incorrect Clip B: %ul vs %ul\n", i, clipa[1], clipb[1]);
			if(memcmp(bufBa,bufBb,cvs[i].b_cmp)!=0) {
				fprintf(stderr, "%i Incorrect Buffer B\n", i);
				for(int j=0;j<16;j++) {
					fprintf(stderr, "%08x %08x %08x\n", *(((uint32_t*)(buf))+j),*(((uint32_t*)(bufBa))+j),*(((uint32_t*)(bufBb))+j));
				}
			}
		}
	}
	free(buf);
}
