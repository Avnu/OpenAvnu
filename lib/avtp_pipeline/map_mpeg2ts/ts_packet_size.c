/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 
1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
Attributions: The inih library portion of the source code is licensed from 
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt. 
Complete license and copyright information can be found at 
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

/*
* MODULE SUMMARY : stand-alone program to detect packet size in file
*/

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define BASE_TS_PACKET_SIZE 188
#define SYNC_BYTE 0x47

#define SCAN_COUNT  300
#define CHECK_COUNT 30

int find_first(FILE* pFile)
{
	int i; char b;
	assert(pFile);
	
	// scan forward to first sync byte
	for (i = 0; i < SCAN_COUNT; i++) {
		b = fgetc(pFile);
		if (b == SYNC_BYTE) {
			fprintf(stderr, "first sync byte at position %d\n", i);
			return i;
		}
	}

	return -1;
}

// Check if packet size is SIZE by looking for the first N sync bytes
int check_size(FILE* pFile, int size, int first)
{
	int i; char b;
	assert(pFile);
	assert(size >= BASE_TS_PACKET_SIZE);
	assert(first >= 0);

	for (i = 0; i < CHECK_COUNT; i++) {
		fseek(pFile, first + i * size, 0);
		b = fgetc(pFile);
		if (b != SYNC_BYTE)
			return 0;
	}
	
	fprintf(stderr, "transport packet size is %d\n", size);
	return size;
}

int 
main(int argc, char *argv[])
{
	FILE *pFile = NULL;
	
	if (argc == 1) {
		pFile = stdin;
	}
	else if (argc == 2) {
		pFile = fopen(argv[1], "rb");
		if (!pFile) {
			fprintf(stderr, "could not open input file: %s\n", argv[1]);
			exit(-2);
		}
	}
	else {
		fprintf(stderr, "error: usage is:\n\t%s [filename]\n", argv[0]);
		exit(-1);
	}

	int first = find_first(pFile);
	if (first < 0) {
		fprintf(stderr, "could not find first sync byte\n");
	}
	else {
		if (check_size(pFile, 188, first))
			;
		else if (check_size(pFile, 192, first))
			;
		else
			fprintf(stderr, "could not determine packet size of the input file\n");
	}

	fclose(pFile);
	pFile = NULL;
	exit(0);
}
