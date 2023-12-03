
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"

FILE _iob[] = {*stdin, *stdout, *stderr};

extern "C" FILE * __cdecl __iob_func(void)
{
    return _iob;
}

UsageEnvironment* env;
char const* inputFileName = "test.amr";
AMRAudioFileSource* audioSource;
RTPSink* audioSink;

void play(); // forward

int main(int argc, char** argv) {
  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);

  // Create 'groupsocks' for RTP and RTCP:
  struct in_addr destinationAddress;
  destinationAddress.s_addr = chooseRandomIPv4SSMAddress(*env);
  // Note: This is a multicast address.  If you wish instead to stream
  // using unicast, then you should use the "testOnDemandRTSPServer"
  // test program - not this test program - as a model.

  const unsigned short rtpPortNum = 16666;
  const unsigned short rtcpPortNum = rtpPortNum+1;
  const unsigned char ttl = 255;

  const Port rtpPort(rtpPortNum);
  const Port rtcpPort(rtcpPortNum);

  Groupsock rtpGroupsock(*env, destinationAddress, rtpPort, ttl);
  rtpGroupsock.multicastSendOnly(); // we're a SSM source
  Groupsock rtcpGroupsock(*env, destinationAddress, rtcpPort, ttl);
  rtcpGroupsock.multicastSendOnly(); // we're a SSM source

  // Create a 'AMR Audio RTP' sink from the RTP 'groupsock':
  audioSink = AMRAudioRTPSink::createNew(*env, &rtpGroupsock, 96);

  // Create (and start) a 'RTCP instance' for this RTP sink:
  const unsigned estimatedSessionBandwidth = 10; // in kbps; for RTCP b/w share
  const unsigned maxCNAMElen = 100;
  unsigned char CNAME[maxCNAMElen+1];
  gethostname((char*)CNAME, maxCNAMElen);
  CNAME[maxCNAMElen] = '\0'; // just in case
  RTCPInstance* rtcp
    = RTCPInstance::createNew(*env, &rtcpGroupsock,
			      estimatedSessionBandwidth, CNAME,
			      audioSink, NULL /* we're a server */,
			      True /* we're a SSM source */);
  // Note: This starts RTCP running automatically

  // Create and start a RTSP server to serve this stream.
  RTSPServer* rtspServer = RTSPServer::createNew(*env, 8554);
  if (rtspServer == NULL) {
    //*env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
      printf("Failed to create RTSP server:%s\n", env->getResultMsg());
    exit(1);
  }

  ServerMediaSession* sms
    = ServerMediaSession::createNew(*env, "testStream", inputFileName,
		   "Session streamed by \"testAMRAudioStreamer\"",
					   True /*SSM*/);
  sms->addSubsession(PassiveServerMediaSubsession::createNew(*audioSink, rtcp));
  rtspServer->addServerMediaSession(sms);

  char* url = rtspServer->rtspURL(sms);
  printf("Play this streaming using the URL:%s\n", url);
  //*env << "Play this stream using the URL \"" << url << "\"\n";
  delete[] url;

  // Start the streaming:
  //*env << "Beginning streaming...\n";
  printf("Beginning streaming...\n");
  play();

  env->taskScheduler().doEventLoop(); // does not return

  return 0; // only to prevent compiler warning
}

void afterPlaying(void* /*clientData*/) {
  //*env << "...done reading from file\n";
printf("...done reading from file\n");

  audioSink->stopPlaying();
  Medium::close(audioSource);
  // Note that this also closes the input file that this source read from.
  play();
}

void play() {
  // Open the input file as an 'AMR audio file source':
  AMRAudioFileSource* audioSource
    = AMRAudioFileSource::createNew(*env, inputFileName);
  if (audioSource == NULL) {
    /**env << "Unable to open file \"" << inputFileName
	 << "\" as an AMR audio file source: "
	 << env->getResultMsg() << "\n";*/
      printf("Unable to open file:%s  as an AMR audio file source:%s\n", inputFileName, env->getResultMsg());
    exit(1);
  }

  // Finally, start playing:
  //*env << "Beginning to read from file...\n";
  printf("Beginning to read from file...\n");
  audioSink->startPlaying(*audioSource, afterPlaying, audioSink);
}
