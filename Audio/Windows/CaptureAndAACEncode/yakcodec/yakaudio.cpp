// yakcodec.cpp : Defines the entry point for the console application.
//
// yakcodec.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

extern "C"  {
#include "libavcodec\avcodec.h"
#include "portaudio.h"
}

#include <csignal>

using namespace std;

#define  SAMPLE_RATE		   (44100)
#define  MS                    (1/1000)  // millisecond.
#define  SAMPLE_CAPTURE_TIME   (10 * MS)
#define  SAMPLES_PER_FRAME     (1024)
//#define  SAMPLES_PER_FRAME     ( SAMPLE_RATE * SAMPLE_CAPTURE_TIME)
#define  NUM_CHANNELS		   (2)

/* Select sample format. */
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"

typedef struct
{
    int          frameIndex;  /* Index into sample array. */
	FILE        *recFileStream; /* Record the stream into File */
	uint8_t     *encoderOutput;
	int         allocOutputSize; /* memory size allocated */
	int         encoderOutputSize;
	PaStream    *recordStream;   /* PaStream opened to record Audio callback. */
	AVCodecContext *c;
} paTestData;

/*******************Global Variables *********************/
paTestData yakData; 

static void yakAudioEncode(paTestData *yakData, uint8_t *rawData, int rawDataSize)
{
    int frameBytes;
	
	frameBytes = yakData->c->frame_size * yakData->c->channels * sizeof(SAMPLE);
	
	// Note: revisit the while loop
	// BUG-1
    while (rawDataSize >= frameBytes)
    {
        int packetSize;

		packetSize = avcodec_encode_audio(yakData->c, yakData->encoderOutput, 
			                              yakData->allocOutputSize, (short *)rawData);
        //printf("EncodePaData (%d-%d-%d)\n",packetSize,audioSize,frameBytes);
	
		yakData->encoderOutputSize  += packetSize;
        rawData += frameBytes;
        rawDataSize -= frameBytes;
    } 
}

static void EncodePaData(paTestData *yakData, uint8_t *rawData, int rawDataSize)
{
	yakAudioEncode(yakData, rawData, rawDataSize);	
	fwrite(yakData->encoderOutput, sizeof(uint8_t), yakData->encoderOutputSize, 
		   yakData->recFileStream);
	yakData->encoderOutputSize = 0;
}

static AVCodecContext *getYakCodec(int sampleRate, int channels, int audioBitrate)
{
    AVCodecContext  *audioCodec;
    AVCodec *codec;

    avcodec_register_all();

    //Set up audio encoder
    codec = avcodec_find_encoder(CODEC_ID_AAC);
    if (codec == NULL) 
	{
		printf("avcodec_find_encoder: ERROR\n");
		return NULL;
	}
    audioCodec = avcodec_alloc_context();
    audioCodec->bit_rate = audioBitrate;
    audioCodec->sample_fmt = SAMPLE_FMT_S16;
    audioCodec->sample_rate = sampleRate;
    audioCodec->channels = channels;
    audioCodec->profile = FF_PROFILE_AAC_MAIN;
    //audioCodec->time_base = (AVRational){1, sampleRate};
	audioCodec->time_base.num  = 1;
	audioCodec->time_base.den  = sampleRate;

    audioCodec->codec_type = AVMEDIA_TYPE_AUDIO;
    if (avcodec_open(audioCodec, codec) < 0) 
	{
		printf("avcodec_open: ERROR\n");
		return NULL;
	}

	return audioCodec;
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
    const SAMPLE *rptr = (const SAMPLE*)inputBuffer;
    int finished = paContinue;
	int audioSize = (framesPerBuffer * sizeof(SAMPLE) * data->c->channels );
	
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
    if( err != paNoError ) 
	    goto done;
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
    if( err != paNoError ) 
	    goto done;
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


int _tmain(int argc, _TCHAR* argv[])
{
	FILE *outstream = fopen("audioout2.aac", "wb");
	PaError err = paNoError;

	if (outstream == NULL) {
        printf("Unable to open file(%s)\n","audioout.aac");
        return -2;
    }

	AVCodecContext  *audioCodec;
    audioCodec = getYakCodec(SAMPLE_RATE, NUM_CHANNELS, 44800);
	if (audioCodec == NULL)
    {
		printf("getYakCodec: Error\n");
        return -3;
    }

	printf("framesize(%d),BitsPerSample(%d)", 
		audioCodec->frame_size, audioCodec->bits_per_raw_sample);
	
	yakData.allocOutputSize = FF_MIN_BUFFER_SIZE * 10;
    yakData.encoderOutput = (uint8_t *)malloc(yakData.allocOutputSize);
	
	if (yakData.encoderOutput == NULL) {
		printf("encoderOutput: malloc Error size(%d)\n",yakData.encoderOutputSize);
        return -4;
	}

	yakData.frameIndex = 0;
	yakData.c = audioCodec;
	yakData.recFileStream = outstream;

	err = yakAudioStreamOpen(&yakData);
	if (paNoError != err)
	{
		printf("Yak PortAudio Record callback init failed(%d)\n",err);
        return -5;
	}

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
		printf("index = %d\n",yakData.frameIndex ); 
		fflush(stdout);	
    }

	printf("Streaming is stopped");
	yakCloseAudioStream(&yakData);

	fclose(outstream);

    return 0;
}

