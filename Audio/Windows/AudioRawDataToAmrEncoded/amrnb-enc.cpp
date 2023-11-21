/* ------------------------------------------------------------------
 * Copyright (C) 2009 Martin Storsjo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */

#include "stdafx.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <interf_enc.h>

#define  SAMPLES_PER_FRAME     (160)
typedef short SAMPLE;

int amrEncodemain(void) {
	int i, j;
	void* amr;
	FILE* out, *in=NULL;
	int sample_pos = 0;
    short buf[SAMPLES_PER_FRAME];
    long nofFrames=0;
    long nofSamples=0;
   
	amr = Encoder_Interface_init(0);
	out = fopen("out.amr", "wb");
	in = fopen("pcm16bit8000.raw", "rb");
	if ( (out == NULL) && (in == NULL)) {
	    printf("file open for read is failed\n");		
		return -1;
	}

	fseek( in, 0L, SEEK_END );
	int file1bytes = ftell( in); //number of data bytes to read
	char* localBuffer = (char *)malloc(file1bytes);
	if ( localBuffer == NULL){
	   return -1;
	}	
	fseek(in,0,SEEK_SET);
	fread(localBuffer,sizeof(char),file1bytes,in);
    fclose(in);
    nofSamples =  file1bytes/ sizeof(SAMPLE);
    nofFrames = (nofSamples/SAMPLES_PER_FRAME);
   
	fwrite("#!AMR\n", 1, 6, out);
	for (i = 0; i < nofFrames; i++) {
        int n=0;
	    uint8_t outbuf[500];
		long offset = i*SAMPLES_PER_FRAME * sizeof(SAMPLE);
        memcpy(buf,&localBuffer[offset], (SAMPLES_PER_FRAME * sizeof(SAMPLE))); 	
		n = Encoder_Interface_Encode(amr, MR475, buf, outbuf, 0);
		printf("Encoder_Interface_Encode: frameSize(%d)-encodedSize(%d)\n",SAMPLES_PER_FRAME,n); 
		fwrite(outbuf, 1, n, out);
	}
	
	fclose(out);
	Encoder_Interface_exit(amr);

	return 0;
}

