extern "C" {
    #include <libavdevice/avdevice.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

#include <string>
#include <iostream>
#include <chrono>

using namespace std::chrono;

//General
std::string output_file = "encoded_video_from_webcam.mp4";
int64_t pts = 0;
const int width = 640;
const int height = 360;
int videoStreamIndex = -1;

//FFMPEG intialization input
AVFormatContext* input_format_context = nullptr;
AVInputFormat* iformat = nullptr;
AVCodecContext  *input_codec_context_video = nullptr;
AVCodec  *input_codec_video = nullptr;
AVStream *istream = nullptr;
int input_fps = 0;
AVRational input_time_base{.num = 0, .den = 0};

//FFMPEG intialization output
AVFormatContext *output_format_context = nullptr;
AVCodecContext  *output_codec_context_video = nullptr;
AVCodec  *output_codec_video = nullptr;
AVStream *stream   = NULL;

int ffmpeg_input_intialization()
{
    input_format_context = avformat_alloc_context();
    if (!input_format_context) {
        std::cout << "failed to alloc memory for AV format Input Context" << std::endl;
        return -1;
    }

    // Register all devices
    avdevice_register_all();

    // Find the video4linux2 input format (for Linux)
    // Use "dshow" for Windows, and "avfoundation" for MacOS
    iformat = const_cast<AVInputFormat*>(av_find_input_format("video4linux2"));

    // Open the input device (webcam)
    if (avformat_open_input(&input_format_context, "/dev/video0", iformat, nullptr) != 0) {
        // Handle error: opening device failed
        return -1;
    }

    if (avformat_find_stream_info(input_format_context, NULL) < 0)
    {
        std::cout << "failed to get stream info" << std::endl;
        return -1;
    }

    // Find the first audio and video stream
    for(int i = 0; i < input_format_context->nb_streams; i++) {
        istream = input_format_context->streams[i];
        if(input_format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStreamIndex = i;
            input_fps = av_q2d(istream->avg_frame_rate);
            input_time_base = istream->time_base;
        }
    }

    if(videoStreamIndex==-1){
        std::cout << "Didn't find a video stream." << std::endl;
        return -1;
    }
    else
    {
        std::cout << "Video Stream detected" << std::endl;
    }

    /* Find a decoder for the  stream. */
    input_codec_video = const_cast<AVCodec *>(avcodec_find_decoder(input_format_context->streams[videoStreamIndex]->codecpar->codec_id));
    if (!input_codec_video) {
        std::cout << "Could not find input codec." << std::endl;
        avformat_close_input(&input_format_context);
        return AVERROR_EXIT;
    }
    else
    {
        std::cout << "The Video decoder is:" << input_codec_video->long_name << std::endl;
    }

    /* Allocate a new decoding context. */
    input_codec_context_video = avcodec_alloc_context3(input_codec_video);
    if (!input_codec_context_video) {
        std::cout << "Could not allocate a decoding context." << std::endl;
        avformat_close_input(&input_format_context);
        return AVERROR(ENOMEM);
    }

    /* Initialize the stream parameters with demuxer information. */
    int error = avcodec_parameters_to_context(input_codec_context_video, (input_format_context)->streams[videoStreamIndex]->codecpar);
    if (error < 0) {
        avformat_close_input(&input_format_context);
        avcodec_free_context(&input_codec_context_video);
        return error;
    }
    else
    {
        std::cout << "Video Stream #"
                << ": codec ID:" << input_codec_context_video->codec_id
                << ", pixel format " << av_get_pix_fmt_name(input_codec_context_video->pix_fmt)
                << ", resolution " << input_codec_context_video->width << "x" << input_codec_context_video->height
                << ", bit rate " << input_codec_context_video->bit_rate << "kb/s"
                << ", fps " << input_fps
                << ", Time Base: " << input_time_base.num << "/" << input_time_base.den
                << std::endl;
    }

    /* Open the decoder for the video stream to use it later. */
    if ((error = avcodec_open2(input_codec_context_video, input_codec_video, NULL)) < 0) {
        std::cout << "Could not open input codec." << std::endl;
        avcodec_free_context(&input_codec_context_video);
        avformat_close_input(&input_format_context);
        return error;
    }
    else
    {
        std::cout << "The Video decoder is opened" << std::endl;
    }
    return 0;
}



int ffmpeg_output_initialization()
{
    avformat_alloc_output_context2(&output_format_context, NULL, NULL, output_file.c_str());
    if(!output_format_context) {
        std::cout << "could not allocate memory for output format" << std::endl;
        return -1;
    }

    stream = avformat_new_stream(output_format_context, NULL);

    output_codec_video = const_cast<AVCodec *>(avcodec_find_encoder(AV_CODEC_ID_H264));
    if (!output_codec_video) {
      std::cout << "could not find the proper codec" << std::endl;
      return -1;
    }

    output_codec_context_video = avcodec_alloc_context3(output_codec_video);
    if(!output_codec_context_video)
    {
      std::cout << "could not allocated memory for codec context" << std::endl;
      return -1;
    }

    output_codec_context_video->height = input_codec_context_video->height;
    output_codec_context_video->width = input_codec_context_video->width;
    output_codec_context_video->sample_aspect_ratio = input_codec_context_video->sample_aspect_ratio;
    output_codec_context_video->pix_fmt = AV_PIX_FMT_YUV420P;

    output_codec_context_video->bit_rate = input_codec_context_video->bit_rate;
    output_codec_context_video->max_b_frames = 3;
    output_codec_context_video->gop_size = input_codec_context_video->gop_size;

    output_codec_context_video->time_base = AVRational{1, input_fps};
    stream->time_base = input_codec_context_video->time_base;

    if (avcodec_open2(output_codec_context_video, output_codec_video, NULL) < 0) {
      std::cout << "could not open the codec" << std::endl;
      return -1;
    }
    avcodec_parameters_from_context(stream->codecpar, output_codec_context_video);

    if ((output_format_context)->oformat->flags & AVFMT_GLOBALHEADER)
        output_format_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;


    if (!(output_format_context->oformat->flags & AVFMT_NOFILE)) {
      if (avio_open(&output_format_context->pb, output_file.c_str(), AVIO_FLAG_WRITE) < 0) {
        std::cout << "could not open the output file" << std::endl;
        return -1;
      }
    }

    int error;
    if ((error = avformat_write_header(output_format_context, NULL)) < 0) {
        std::cout << "Could not write output file header." << std::endl;
        return error;
    }
    else
    {
        std::cout << "Output file header info written" << std::endl;
    }    

    return 0;
}

int encode_video(AVFrame *input_frame)
{
    if (input_frame) input_frame->pict_type = AV_PICTURE_TYPE_NONE;
  
    AVPacket *output_packet = av_packet_alloc();
    if (!output_packet) {
      std::cout << "could not allocate memory for output packet" << std::endl;
      return -1;
    }
  
    int response = avcodec_send_frame(output_codec_context_video, input_frame);
  
    while (response >= 0) {
        response = avcodec_receive_packet(output_codec_context_video, output_packet);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
          break;
        } else if (response < 0) {
          std::cout << "Error while receiving packet from encoder:" << std::endl;
          return -1;
        }
    
        output_packet->stream_index = videoStreamIndex;
        output_packet->duration = output_format_context->streams[videoStreamIndex]->time_base.den / output_format_context->streams[videoStreamIndex]->time_base.num / input_format_context->streams[videoStreamIndex]->avg_frame_rate.num * input_format_context->streams[videoStreamIndex]->avg_frame_rate.den; 
    
        av_packet_rescale_ts(output_packet, input_format_context->streams[videoStreamIndex]->time_base, output_format_context->streams[videoStreamIndex]->time_base);
        response = av_interleaved_write_frame(output_format_context, output_packet);
        if(response != 0) {
          std::cout << "Error " << response <<  "receiving packet from decoder:" << std::endl; //av_err2str(response) << endl;
          return -1;
        }
    }
    av_packet_unref(output_packet);
    av_packet_free(&output_packet);
    return 0;
}

// Function to convert the frame and copy properties
AVFrame* convert_frame(AVFrame* source_frame, enum AVPixelFormat dest_format) {
    // Allocate the destination frame
    AVFrame* dest_frame = av_frame_alloc();
    dest_frame->format = dest_format;
    dest_frame->width  = source_frame->width;
    dest_frame->height = source_frame->height;

    if (av_frame_copy_props(dest_frame, source_frame) < 0) {
        std::cout << "Error Copying" << std::endl;
        return nullptr;
    }

    // Allocate memory for the new frame data
    av_image_alloc(dest_frame->data, dest_frame->linesize, dest_frame->width, dest_frame->height, dest_format, 32);

    // Create the scaling context
    SwsContext* sws_ctx = sws_getContext(
        source_frame->width, source_frame->height, (enum AVPixelFormat)source_frame->format,
        dest_frame->width, dest_frame->height, dest_format,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    // Perform the conversion
    sws_scale(sws_ctx, (const uint8_t* const*)source_frame->data, source_frame->linesize, 0, source_frame->height,
              dest_frame->data, dest_frame->linesize);

    // Free the scaling context
    sws_freeContext(sws_ctx);

    return dest_frame;
}


int main(int argc, char *argv[]) {

    ffmpeg_input_intialization();

    ffmpeg_output_initialization();

   AVFrame *iframe = av_frame_alloc();
   if (!iframe) {
        fprintf(stderr, "Could not allocate video frame\n");
        return -1;
    }

    AVPacket packet;

    // Initialize packet
    av_init_packet(&packet);
    uint64_t counter = 0;

    int64_t initial_pts = AV_NOPTS_VALUE;

    auto start = steady_clock::now();

    // Read frames from the device
    while (av_read_frame(input_format_context, &packet) >= 0) {
        // Here you can process the frame (packet.data)
        // For example, saving it, encoding it, etc.
        if(input_format_context->streams[packet.stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            if (initial_pts == AV_NOPTS_VALUE) {
                initial_pts = packet.pts; // Store the initial PTS
            }
            // Adjust PTS to start from 0
            if (packet.pts != AV_NOPTS_VALUE) {
                packet.pts -= initial_pts;
            }
            if (packet.dts != AV_NOPTS_VALUE) {
                packet.dts -= initial_pts;
            }
            int response = avcodec_send_packet(input_codec_context_video, &packet);
            if (response < 0) {
                std::cout << "Error while sending packet to decoder:" << std::endl;
                return response;
            }
            while (response >= 0) {
                response = avcodec_receive_frame(input_codec_context_video, iframe);
                if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                  break;
                } else if (response < 0) {
                  std::cout << "Error while receiving frame from decoder:" << std::endl;
                  return response;
                }

                AVFrame *dframe = convert_frame(iframe, AV_PIX_FMT_YUV420P);

                int frame_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, dframe->width, dframe->height, 1);

                // Print information about the frame
                std::cout << "FRAME DEST: " << ++counter
                << " PTS: " << dframe->pts
                << " DTS: " << dframe->pkt_dts
                << " Size: " << frame_size
                << " width: " << dframe->width
                << " height: " << dframe->height
                << " pix_format" << dframe->format << std::endl;

                if (response >= 0) {
                  if (encode_video(dframe)) return -1;
                }
                av_frame_unref(iframe);
                av_frame_unref(dframe);
            }
        }

        // Remember to unref the packet once you're done with it
        av_packet_unref(&packet);
        auto now = steady_clock::now();
        auto elapsed = duration_cast<seconds>(now - start);
        if (elapsed.count() > std::stoi(argv[1])) {
            std::cout << "5 seconds have elapsed. Exiting loop." << std::endl;
            break;
        }
    }

    encode_video(NULL);

    int error;
    if ((error = av_write_trailer(output_format_context)) < 0) {
        std::cout << "Could not write output file trailer." << std::endl;
        return error;
    }    

    if(iframe)
        av_frame_free(&iframe);

    if (output_format_context) {
        avio_closep(&output_format_context->pb);
        avformat_free_context(output_format_context);
    }
    if (input_format_context) {
        avio_closep(&input_format_context->pb);
        avformat_free_context(input_format_context);
    }    

    if (output_codec_context_video)
        avcodec_free_context(&output_codec_context_video);
    if (input_codec_context_video)
        avcodec_free_context(&input_codec_context_video);

    return 0;
}
