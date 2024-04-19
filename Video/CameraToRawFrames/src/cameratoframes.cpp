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
int videoStreamIndex = -1;

//FFMPEG intialization input
AVFormatContext* input_format_context = nullptr;
AVInputFormat* iformat = nullptr;
AVCodecContext  *input_codec_context_video = nullptr;
AVCodec  *input_codec_video = nullptr;
AVStream *istream = nullptr;
int input_fps = 0;
AVRational input_time_base{.num = 0, .den = 0};

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

void ppm_save(unsigned char* buf, int wrap, int xsize, int ysize, const char* filename)
{
    FILE* f;
    int i;

    f = fopen(filename, "wb");
    if (!f) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    fprintf(f, "P6\n%d %d\n%d\n", xsize, ysize, 255);

    for (i = 0; i < ysize; i++)
    {
        fwrite(buf + i * wrap, 1, xsize * 3, f);
    }

    fclose(f);
}

void save_frame_as_image(AVFrame* frame, const char* filename) {
    struct SwsContext* sws_ctx = nullptr;

    // Create a RGB frame for storing the converted image
    AVFrame* rgb_frame = av_frame_alloc();
    rgb_frame->format = AV_PIX_FMT_RGB24;
    rgb_frame->width = frame->width;
    rgb_frame->height = frame->height;

    // Allocate buffer for the converted image
    av_frame_get_buffer(rgb_frame, 32);

    // Initialize SwScale context for the conversion
    sws_ctx = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format,
        rgb_frame->width, rgb_frame->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!sws_ctx) {
        std::cerr << "Error creating SwScale context" << std::endl;
        return;
    }

    // Perform the color conversion
    sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height,
        rgb_frame->data, rgb_frame->linesize);

    // Save the RGB frame as an image
    ppm_save(rgb_frame->data[0], rgb_frame->linesize[0], rgb_frame->width, rgb_frame->height, filename);

    // Free allocated resources
    av_frame_free(&rgb_frame);
    sws_freeContext(sws_ctx);
}

int main(int argc, char *argv[]) {

    ffmpeg_input_intialization();

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

                int frame_size = av_image_get_buffer_size(AV_PIX_FMT_YUYV422, iframe->width, iframe->height, 1);

                // Print information about the frame
                std::cout << "FRAME DEST: " << ++counter
                << " PTS: " << iframe->pts
                << " DTS: " << iframe->pkt_dts
                << " Size: " << frame_size
                << " width: " << iframe->width
                << " height: " << iframe->height
                << " pix_format:" << iframe->format << std::endl;

                std::string file_path_pgm = "/home/phani/MMWorkSpace/VideoBasics/CameraToRawFrames/rawFrames/pgm-" + std::to_string(counter) + ".ppm";
                save_frame_as_image(iframe, file_path_pgm.c_str());

                av_frame_unref(iframe);
            }
        }

        // Remember to unref the packet once you're done with it
        av_packet_unref(&packet);
        auto now = steady_clock::now();
        auto elapsed = duration_cast<seconds>(now - start);
        if (elapsed.count() > std::stoi(argv[1])) {
            std::cout << "Exiting loop." << std::endl;
            break;
        }
    }

    if(iframe)
        av_frame_free(&iframe);

    if (input_format_context) {
        avio_closep(&input_format_context->pb);
        avformat_free_context(input_format_context);
    }

    if (input_codec_context_video)
        avcodec_free_context(&input_codec_context_video);

    return 0;
}
