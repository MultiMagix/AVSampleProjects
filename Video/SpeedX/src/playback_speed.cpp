extern "C" {
    #include <libavdevice/avdevice.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
    #include <libavfilter/avfilter.h>
    #include <libavfilter/buffersrc.h>
    #include <libavfilter/buffersink.h>    
}

#include <string>
#include <iostream>
#include <chrono>

using namespace std::chrono;

//General
std::string input_file  = "";
std::string output_file = "speedx.mp4";
static int64_t counter = 0;
int videoStreamIndex = -1;
double speed_x = 0;

//FFMPEG intialization input
AVFormatContext* input_format_context = nullptr;
AVCodecContext  *input_codec_context_video = nullptr;
AVCodec  *input_codec_video = nullptr;
AVStream *istream = nullptr;

int input_fps = 0;
AVRational input_time_base{.num = 0, .den = 0};

//FFMPEG intialization output
AVFormatContext *output_format_context = nullptr;
AVCodecContext  *output_codec_context_video = nullptr;
AVCodec  *output_codec_video = nullptr;
AVStream *stream   = nullptr;

//FFMPEG Filter Graph
AVFilterGraph* filter_graph = nullptr;
AVFilterContext* buffersrc_ctx = nullptr;
AVFilterContext* buffersink_ctx = nullptr;

int ffmpeg_filter_graph_intialization(double speedx) {
    char args[512];
    int ret = 0;

    filter_graph = avfilter_graph_alloc();
    if (!filter_graph) {
        std::cerr << "Could not allocate filter graph.\n";
        return -1;
    }

    snprintf(args, sizeof(args),
         "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d",
         input_codec_context_video->width, input_codec_context_video->height, input_codec_context_video->pix_fmt,
         input_codec_context_video->time_base.num, input_codec_context_video->time_base.den);


    if (avfilter_graph_create_filter(&buffersrc_ctx, avfilter_get_by_name("buffer"), "src",
                                     args, nullptr, filter_graph) < 0) {
        std::cerr << "Cannot create buffer source\n";
        return -1;
    }

    if (avfilter_graph_create_filter(&buffersink_ctx, avfilter_get_by_name("buffersink"), "sink",
                                     nullptr, nullptr, filter_graph) < 0) {
        std::cerr << "Cannot create buffer sink\n";
        return -1;
    }

    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs  = avfilter_inout_alloc();
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = nullptr;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = nullptr;

    std::string pts_value = std::to_string(speedx);
    std::string pts_config = "setpts=" + pts_value + "*PTS";

    std::cout << "PTS CONFIGURATION:" << pts_config << std::endl;

    if (avfilter_graph_parse_ptr(filter_graph, pts_config.c_str(), &inputs, &outputs, nullptr) < 0) {
        std::cerr << "Failed to parse filter graph\n";
        return -1;
    }

    if (avfilter_graph_config(filter_graph, nullptr) < 0) {
        std::cerr << "Failed to configure filter graph\n";
        return -1;
    }

    std::cout << "Filter Graph intialization is successful" << std::endl;

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return 0;
}


int ffmpeg_input_intialization()
{
    input_format_context = avformat_alloc_context();
    if (!input_format_context) {
        std::cout << "failed to alloc memory for AV format Input Context" << std::endl;
        return -1;
    }

    // Open the input device (webcam)
    if (avformat_open_input(&input_format_context, input_file.c_str(), nullptr, nullptr) != 0) {
        // Handle error: opening device failed
        return -1;
    }

    if (avformat_find_stream_info(input_format_context, NULL) < 0)
    {
        std::cout << "failed to get stream info" << std::endl;
        return -1;
    }

    // Find the first video stream
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
        input_codec_context_video->time_base = input_time_base;
        std::cout << "Video Stream #"
                << ": codec ID:" << input_codec_context_video->codec_id
                << ", pixel format " << av_get_pix_fmt_name(input_codec_context_video->pix_fmt)
                << ", resolution " << input_codec_context_video->width << "x" << input_codec_context_video->height
                << ", bit rate " << input_codec_context_video->bit_rate << "kb/s"
                << ", fps " << input_fps
                << ", Time Base: " << input_codec_context_video->time_base.num << "/" << input_codec_context_video->time_base.den
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
    output_codec_context_video->pix_fmt = input_codec_context_video->pix_fmt;

    output_codec_context_video->bit_rate = input_codec_context_video->bit_rate;
    output_codec_context_video->max_b_frames = 1;
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

int main(int argc, char *argv[]) {

    if(argc < 4)
    {
        std::cout << "Not enough args passed" << std::endl;
        return 0;
    }

    input_file = argv[1];
    std::string speed_type = argv[2];
    std::string speed_value = argv[3];

    if(speed_type == "fast")
        speed_x = 1/ std::stod(speed_value);
    else if(speed_type == "slow")
        speed_x = std::stod(speed_value);

    ffmpeg_input_intialization();

    ffmpeg_output_initialization();

    ffmpeg_filter_graph_intialization(speed_x);

   AVFrame *iframe = av_frame_alloc();
   if (!iframe) {
        fprintf(stderr, "Could not allocate video frame\n");
        return -1;
    }

    AVFrame* filt_frame = av_frame_alloc();
   if (!filt_frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        return -1;
    }    

    AVPacket packet;

    // Initialize packet
    av_init_packet(&packet);

    // Read frames from the device
    while (av_read_frame(input_format_context, &packet) >= 0) {
        // Here you can process the frame (packet.data)
        // For example, saving it, encoding it, etc.
        if(input_format_context->streams[packet.stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
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

                iframe->pts = iframe->best_effort_timestamp;
                if (av_buffersrc_add_frame_flags(buffersrc_ctx, iframe, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
                    std::cerr << "Error while feeding the filter graph\n";
                    break;
                }              

                while (1) {
                    response = av_buffersink_get_frame(buffersink_ctx, filt_frame);
                    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
                        break;
                    if (response < 0)
                        return response;


                    int frame_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, filt_frame->width, filt_frame->height, 1);

                    // Print information about the frame
                    std::cout << "FRAME: " << ++counter
                    << " PTS: " << filt_frame->pts
                    << " DTS: " << filt_frame->pkt_dts
                    << " Size: " << frame_size
                    << " width: " << filt_frame->width
                    << " height: " << filt_frame->height
                    << " pix_format" << filt_frame->format << std::endl;

                    filt_frame->pts = av_rescale_q(filt_frame->pts, av_buffersink_get_time_base(buffersink_ctx), output_format_context->streams[videoStreamIndex]->time_base);

                    if (encode_video(filt_frame)) return -1;
                }                                 
                av_frame_unref(iframe);
                av_frame_unref(filt_frame);
            }
        }

        // Remember to unref the packet once you're done with it
        av_packet_unref(&packet);
    }

    encode_video(NULL);

    int error;
    if ((error = av_write_trailer(output_format_context)) < 0) {
        std::cout << "Could not write output file trailer." << std::endl;
        return error;
    }    

    if(iframe)
        av_frame_free(&iframe);

    if(filt_frame)
        av_frame_free(&filt_frame);

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
