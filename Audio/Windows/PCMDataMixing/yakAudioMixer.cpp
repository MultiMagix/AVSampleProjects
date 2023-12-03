// yakmix.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>


/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE  (44100)
#define FRAME_TIME_MS  (10)  // 10 milliSeconds.
//#define SAMPLES_PER_FRAME (1024)
#define SAMPLES_PER_FRAME (SAMPLE_RATE/FRAME_TIME_MS)

/* Select sample format. */
typedef short SAMPLE;
//typedef char SAMPLE;

/*
 * PCM Raw 16 Bit, Signed.
 */
SAMPLE yakMixOneSAMPLE(SAMPLE sample1, SAMPLE sample2)
{
	//normalization to save data range
	float samplef1 = sample1 / 32768.0f;
	float samplef2 = sample2 / 32768.0f;
	float mixed = samplef1 + samplef2;

	// reduce the volume a bit:
	mixed *= (float)0.9;

	// soft clipping
	if (mixed > 1.0f) mixed = 1.0f;
	if (mixed < -1.0f) mixed = -1.0f;


	SAMPLE outputSample = (SAMPLE)(mixed * 32768.0f); //de-normalization
	return outputSample;
}

int yakMixOneFrame(long frameSize, SAMPLE *outFrame, long noch, SAMPLE *frame1, SAMPLE *frame2)
{
	long samplesPerFrame = frameSize  * noch; /* no of channels * framesize */
	for (int i=0;i<samplesPerFrame;i++) {
       outFrame[i] =  yakMixOneSAMPLE(frame1[i],frame2[i]);
	}
	return 0;
}

void writeToFile(FILE *wid, SAMPLE *mixedFrame, long samplesPerFrame)
{
	for (int i=0;i<samplesPerFrame;i++) {
		fwrite( &mixedFrame[i], sizeof(SAMPLE), 1, wid);
	}
}

int MixAudioStreams(FILE *rid1, FILE *rid2,FILE *wid)
{
   int i=0;
   SAMPLE mixerFrame[SAMPLES_PER_FRAME];
   long nofFrames=0;
   long nofSamples=0;
   long nofbytes=0;

   fseek( rid1, 0L, SEEK_END );
   int file1bytes = ftell( rid1); //number of data bytes to read
   char* localBuffer1 = (char *)malloc(file1bytes);
   if ( localBuffer1 == NULL){
	   return -1;
   }
   fseek(rid1,0,SEEK_SET);


   fseek( rid2, 0L, SEEK_END );
   int file2bytes = ftell( rid2); //number of data bytes to read
   char* localBuffer2 = (char *)malloc(file2bytes);
   if ( localBuffer2 == NULL){
	   return -1;
   }
   fseek(rid2,0,SEEK_SET);
   
   fread(localBuffer1,sizeof(char),file1bytes,rid1);
   fread(localBuffer2,sizeof(char),file2bytes,rid2);

   nofbytes=(file1bytes <file2bytes)?file1bytes:file2bytes;
   nofSamples =  nofbytes/ sizeof(SAMPLE);
   nofFrames = (nofSamples/SAMPLES_PER_FRAME);
   for ( i=0;i<nofFrames;i++)
   {
	   SAMPLE *sPtr1 = (SAMPLE *) ( localBuffer1+ (i * SAMPLES_PER_FRAME * sizeof(SAMPLE)));
	   SAMPLE *sPtr2 = (SAMPLE *) ( localBuffer2+ (i * SAMPLES_PER_FRAME * sizeof(SAMPLE)));

	   yakMixOneFrame(SAMPLES_PER_FRAME,mixerFrame, 1,sPtr1, sPtr2);
	   writeToFile(wid, mixerFrame, SAMPLES_PER_FRAME);
   }

   return 0;
}

int main(int argc, char* argv[])
{
	int i=0;
	FILE  *frid1, *frid2, *fwid;
	errno_t ferrno;

	if ( argc != 3) 
	{

		printf("\n"
			   "\tUsage:  \n"
               "\t*********\n"
               "\tyakmix.exe  pcmFile1.raw pcmFile2.raw \n"
               "\t**************************************\n");
	    //return -1;
		argv[1] = "pcm16raw1.pcm";
		argv[2] = "pcm16raw2.pcm";
	}

	for (i=0;i<argc;i++)
		printf("[%d] = %s\n",i, argv[i]);

	ferrno = fopen_s( &frid1, argv[1], "rb" );
    if(ferrno!=0) {
		printf("Could not open file(%s)\n", argv[1]);
        return -1;
    }

	ferrno = fopen_s( &frid2, argv[2], "rb" );
    if(ferrno!=0) {
		printf("Could not open file(%s)\n", argv[2]);
        return -1;
    }

	ferrno = fopen_s( &fwid, "pcmMix16Raw.pcm", "wb" );
    if(ferrno!=0) {
        printf("Could not open file(%s).", "pcmMix16Raw.pcm");
		exit(-1);
    }

	MixAudioStreams(frid1, frid2, fwid);

	fclose(frid1);
	fclose(frid2);
	fclose(fwid);

	printf("\n");
	return 0;
}