// yakcodec.cpp : Defines the entry point for the console application.
//
// yakcodec.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <interf_enc.h>
#include <windows.h>
#include <process.h>     // needed for _beginthread()

#include <time.h>

extern "C"  {
#include "portaudio.h"
}

#include <iostream>
#include <csignal>
#include <thread>
#include <chrono>

//#define __CONFIG_RTP__  1

using namespace std;

#define  SAMPLE_RATE		   (8000)
#define  SAMPLES_PER_FRAME     (160)
#define  NUM_CHANNELS		   (1)
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"

#define AMR_ENCODED_FRAME_SIZE 500
#define AMR_ENCODED_SIZE_PER_FRAME 13

typedef struct _RTP_PKT
{
	uint8_t hdr[13];
	uint8_t amrEncodedData[AMR_ENCODED_FRAME_SIZE];

} RTP_PKT;

typedef struct
{
    int           frameIndex;  /* Index into sample array. */
	unsigned int  mSeqNo;
	FILE          *recFileStream; /* Record the stream into File */
	PaStream      *recordStream;   /* PaStream opened to record Audio callback. */
	void          *amr;
	RTP_PKT       rtp;
	int           rtpSize;
	int           amrEncodedDataSize;
} paTestData;

/*******************Global Variables *********************/
paTestData yakData; 

static unsigned short rtpSequenceNo = 0;

static void yakAudioEncode(paTestData *yakData, const short *rawData, int rawDataSize)
{
	yakData->amrEncodedDataSize  = Encoder_Interface_Encode(yakData->amr, MR475, rawData, yakData->rtp.amrEncodedData, 0);
	//printf("Encoder_Interface_Encode: frameSize(%d)-encodedSize(%d)\n", SAMPLES_PER_FRAME, yakData->amrEncodedDataSize);
	//std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

static void setRTPTS(paTestData *yakData, int value) {
	yakData->rtp.hdr[4] = (char)(value >> 24);
	yakData->rtp.hdr[5] = (char)(value >> 16);
	yakData->rtp.hdr[6] = (char)(value >> 8);
	yakData->rtp.hdr[7] = (char)(value);
}

static void setRTPSequenceNo(paTestData *yakData, short value) {
	yakData->rtp.hdr[2] = (char)(value >> 8);
	yakData->rtp.hdr[3] = (char)(value);
}

static void fillRTPHeaderInit(paTestData *yakData)
{
	uint8_t *hdr = yakData->rtp.hdr;

	// control bytes.(2 bytes).
	hdr[0] = 0x80;
	hdr[1] = 0x63;

	// sequence number will be updated with each RTP Packet.(2 Bytes).
	hdr[2] = 0x00;
	hdr[3] = 0x00;

	// TimeStamp will be updated with each RTP packet.(4 Bytes).
	hdr[4] = 0x00;
	hdr[5] = 0x00;
	hdr[6] = 0x00;
	hdr[7] = 0x00;

	// Source Id Fixed ( 4 bytes).
	hdr[8]  = 0xBE;
	hdr[9]  = 0xAD;
	hdr[10] = 0xFA;
	hdr[11] = 0xCE;

	// mystry byte( copied from Media server AMR decoding).
	hdr[12] = 0xF0;
}

#ifdef __CONFIG_RTP__
static void fillRTPHeader(paTestData *yakData)
{
	unsigned int ts=0;
	struct timeval timeNow;
	gettimeofday(&timeNow, NULL);
	ts = timeNow.tv_sec*1000000 + timeNow.tv_usec;

	rtpSequenceNo++;

	setRTPSequenceNo(yakData, rtpSequenceNo);
	setRTPTS(yakData, ts);
}
#endif


static void EncodePaData(paTestData *yakData, uint8_t *rawData, int rawDataSize)
{
	uint8_t *data;
	data = yakData->rtp.amrEncodedData;
	uint32_t pktSize = 0;

	yakAudioEncode(yakData, (const short *)rawData, rawDataSize);	
#ifdef __CONFIG_RTP__
	fillRTPHeader(yakData);
#endif
	fwrite(yakData->rtp.amrEncodedData, sizeof(uint8_t), yakData->amrEncodedDataSize, 
		   yakData->recFileStream);
	pktSize = AMR_ENCODED_SIZE_PER_FRAME + yakData->amrEncodedDataSize ;
		
	// SendAudioData((uint8_t *) yakData->rtp, pktSize);
#if 0
	printf("Data[%d %d %d %d|%d %d %d %d || %d %d %d %d|%d]\n", 
 		(uint8_t)data[0], (uint8_t)data[1], (uint8_t)data[2], (uint8_t)data[3], 
		(uint8_t)data[4], (uint8_t)data[5], (uint8_t)data[6], (uint8_t)data[7], 
		(uint8_t)data[8], (uint8_t)data[9], (uint8_t)data[10],(uint8_t)data[11], 
		(uint8_t)data[12]);
#endif
	yakData->amrEncodedDataSize = 0;
}


bool capStopFlag=false;

void signalHandler( int signum )
{
   printf("Signal(Cntl-C)\n");    
   capStopFlag = true;
   //exit(signum);  
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may be called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int yakAudioRecordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    paTestData *data = (paTestData*)userData;
	int finished = paContinue;
	int audioSize = (framesPerBuffer * sizeof(SAMPLE) * NUM_CHANNELS );
	
    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

	if (capStopFlag ) {
        finished = paComplete;
    }   

    EncodePaData(data, (uint8_t *) inputBuffer, audioSize);
	data->frameIndex++;
    return finished;
}

static PaError yakAudioStreamOpen(paTestData *yakData)
{
    PaStreamParameters  inputParameters;
    PaStream*           stream;
    PaError             err = paNoError;

    // register signal SIGINT and signal handler
	signal(SIGINT, signalHandler);

    err = Pa_Initialize();
    if( err != paNoError ) goto done;

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        goto done;
    }
    inputParameters.channelCount = NUM_CHANNELS;                    /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;
    
    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              NULL,                  /* &outputParameters, */
              SAMPLE_RATE,
              SAMPLES_PER_FRAME,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              yakAudioRecordCallback,
              yakData );
    if( err != paNoError ) goto done;

	yakData->recordStream = stream; 
	return paNoError;

done:
    Pa_Terminate();
    if( err != paNoError )
    {
        fprintf( stderr, "An error occured while using the portaudio stream\n" );
        fprintf( stderr, "Error number: %d\n", err );
        fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
        err = 1;          /* Always return 0 or 1, but no other return codes. */
    }
    return err;
}

static PaError yakAudioStreamStart(paTestData *yakData) 
{
	return Pa_StartStream(yakData->recordStream);
}

static PaError isYakAudioStreamNotStopped(paTestData *yakData) 
{
	fflush(stdout);
	return Pa_IsStreamActive(yakData->recordStream);
}

static PaError yakCloseAudioStream(paTestData *yakData) 
{	
	return Pa_CloseStream( yakData->recordStream);
}

int yakAudioMain(void)
{
	FILE *outstream;
	PaError err = paNoError;
	errno_t ferrno; 
	ferrno = fopen_s( &outstream, "out.amr", "wb" );
    if(ferrno!=0) {
		printf("Could not open file(%s)\n", "audioout2.aac");
        return -1;
    }

	void *amr = Encoder_Interface_init(0);
	if (amr == NULL)
    {
		printf("getYakCodec: Error\n");
        return -3;
    }


	//Store details in Audio State
	yakData.mSeqNo =0;
	yakData.frameIndex = 0;
	yakData.recFileStream = outstream;
	yakData.amr = amr;
    
	// RTP Header Init.
	fillRTPHeaderInit(&yakData);

	err = yakAudioStreamOpen(&yakData);
	if (paNoError != err)
	{
		printf("Yak PortAudio Record callback init failed(%d)\n",err);
        return -5;
	}

	fwrite("#!AMR\n", 1, 6, outstream);

	err = yakAudioStreamStart(&yakData);
	if (paNoError != err)
	{
		printf("yakAudioStreamStart failed(%d)\n",err);
        return -6;
	}

	printf("\n=== Now recording!! Please speak into the microphone. ===\n"); 
    while (isYakAudioStreamNotStopped(&yakData))
    {
		Pa_Sleep(1000); /* milliseconds */
		//printf("index = %d\n",yakData.frameIndex ); 
		fflush(stdout);	
    }

	printf("Streaming is stopped");
	yakCloseAudioStream(&yakData);
	Encoder_Interface_exit(yakData.amr);
	fclose(outstream);

    return 0;
}


/*******************************************************************/
void AudioThreadProc(void *param)
{
	int h=*((int*)param);
    printf("%d AudioThreadProc is Running!\n",h);

	yakAudioMain();
	_endthread();
}

void CaptureAudio(void)
{
	int dummy=111;
	HANDLE handle;

	handle = (HANDLE) _beginthread( AudioThreadProc,0,&dummy); // create thread
    WaitForSingleObject(handle,INFINITE);
}



int _tmain(int argc, _TCHAR* argv[])
{
	CaptureAudio();
	return 0;
}