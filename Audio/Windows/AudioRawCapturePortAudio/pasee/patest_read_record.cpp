/** @file patest_read_record.c
	@ingroup test_src
	@brief Record input into an array; Save array to a file; Playback recorded
    data. Implemented using the blocking API (Pa_ReadStream(), Pa_WriteStream() )
	@author Phil Burk  http://www.softsynth.com
    @author Ross Bencina rossb@audiomulch.com
*/
/*
 * $Id: patest_read_record.c 1368 2008-03-01 00:38:27Z rossb $
 *
 * This program uses the PortAudio Portable Audio Library.
 * For more information see: http://www.portaudio.com
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * The text above constitutes the entire PortAudio license; however, 
 * the PortAudio community also makes the following non-binding requests:
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version. It is also 
 * requested that these non-binding requests be included along with the 
 * license above.
 */
#include "stdafx.h"

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include "portaudio.h"
}

/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE  (44100)
#define FRAMES_PER_BUFFER (1024)
#define NUM_SECONDS     (30)
#define NUM_CHANNELS    (2)
/* #define DITHER_FLAG     (paDitherOff)  */
#define DITHER_FLAG     (0) /**/

/* Select sample format. */
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"

/*******************************************************************/
int prr_main(void)
{
    PaStreamParameters inputParameters, outputParameters;
    PaStream *stream;
    PaError err;
    SAMPLE *recordedSamples;
    int i;
    int totalFrames;
    int numSamples;
    int numBytes;    
    
    printf("patest_read_record.c\n"); fflush(stdout);

    totalFrames = NUM_SECONDS * SAMPLE_RATE; /* Record for a few seconds. */
    numSamples = totalFrames * NUM_CHANNELS;

    numBytes = numSamples * sizeof(SAMPLE);
    recordedSamples = (SAMPLE *) malloc( numBytes );
    if( recordedSamples == NULL )
    {
        printf("Could not allocate record array.\n");
        exit(1);
    }
    for( i=0; i<numSamples; i++ ) recordedSamples[i] = 0;

    err = Pa_Initialize();
    if( err != paNoError ) goto error;

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
      fprintf(stderr,"Error: No default input device.\n");
      goto error;
    }
    inputParameters.channelCount = NUM_CHANNELS;
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              NULL,                  /* &outputParameters, */
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              NULL, /* no callback, use blocking API */
              NULL ); /* no callback, so no callback userData */
    if( err != paNoError ) goto error;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto error;
    printf("Now recording!!\n"); fflush(stdout);

    err = Pa_ReadStream( stream, recordedSamples, totalFrames );
    if( err != paNoError ) goto error;
    
    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto error;

    /* Write recorded data to a file. */
#if 1
    {
        FILE  *fid;
        fid = fopen("recorded.raw", "wb");
        if( fid == NULL )
        {
            printf("Could not open file.");
        }
        else
        {
            fwrite( recordedSamples, NUM_CHANNELS * sizeof(SAMPLE), totalFrames, fid );
            fclose( fid );
            printf("Wrote data to 'recorded.raw'\n");
        }
    }
#endif

    /* Playback recorded data.  -------------------------------------------- */
    
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    if (outputParameters.device == paNoDevice) {
      fprintf(stderr,"Error: No default output device.\n");
      goto error;
    }
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    printf("Begin playback.\n"); fflush(stdout);

    err = Pa_OpenStream(
              &stream,
              NULL, /* no input */
              &outputParameters,
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              NULL, /* no callback, use blocking API */
              NULL ); /* no callback, so no callback userData */
    if( err != paNoError ) goto error;

    if( stream )
    {
        err = Pa_StartStream( stream );
        if( err != paNoError ) goto error;
        printf("Waiting for playback to finish.\n"); fflush(stdout);

        err = Pa_WriteStream( stream, recordedSamples, totalFrames );
        if( err != paNoError ) goto error;

        err = Pa_CloseStream( stream );
        if( err != paNoError ) goto error;
        printf("Done.\n"); fflush(stdout);
    }
    free( recordedSamples );

    Pa_Terminate();
    return 0;

error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return -1;
}

