extern "C" {
    #include "libavformat/avformat.h"
    #include "libavcodec/avcodec.h"
    #include "libswscale/swscale.h"
    #include "libavutil/imgutils.h"
}

#include <iostream>
#include <ctime>
#include <fstream>

using namespace std;

static int counter = 0;
static int counter2 = 0;
static int counter3 = 0;

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


int main(int argc, char** argv) {
    if (argc != 2) {
        cout << "Usage: " << argv[0] << " <input_video_file>" << endl;
        return 1;
    }

    const char* inputFilename = argv[1];

    // Open the input file
    AVFormatContext* formatContext = avformat_alloc_context();
    if (!formatContext) {
        std::cout << "failed to alloc memory for AV format Input Context" << std::endl;
        return -1;
    }

    if (avformat_open_input(&formatContext, inputFilename, NULL, NULL) != 0) {
        cerr << "Error: Could not open the file" << endl;
        return 1;
    }

    // Find the video stream information
    if (avformat_find_stream_info(formatContext, NULL) < 0) {
        cerr << "Error: Could not find the stream information" << endl;
        avformat_close_input(&formatContext);
        return 1;
    }

    // Find the first video stream
    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        cerr << "Error: Could not find a video stream" << endl;
        avformat_close_input(&formatContext);
        return 1;
    }
    else
    {
        std::cout << "Video Stream detected" << std::endl;
    }    

    // Get the codec parameters for the video stream
    AVCodecParameters* codecParameters = formatContext->streams[videoStreamIndex]->codecpar;

    // Find the decoder for the video stream
    AVCodec* codec = const_cast<AVCodec *>(avcodec_find_decoder(codecParameters->codec_id));
    if (!codec) {
        cerr << "Error: Unsupported codec" << endl;
        avformat_close_input(&formatContext);
        return 1;
    }

    // Open the codec
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(codecContext, codecParameters) < 0) {
        cerr << "Error: Failed to copy codec parameters to codec context" << endl;
        avcodec_close(codecContext);
        avformat_close_input(&formatContext);
        return 1;
    }

    int error = avcodec_parameters_to_context(codecContext, (formatContext)->streams[videoStreamIndex]->codecpar);
    if (error < 0) {
        avformat_close_input(&formatContext);
        avcodec_free_context(&codecContext);
        return error;
    }
    else
    {
        std::cout << "Video Stream #"
                << ": codec ID:" << codecContext->codec_id
                << ", pixel format " << av_get_pix_fmt_name(codecContext->pix_fmt)
                << ", resolution " << codecContext->width << "x" << codecContext->height
                << ", bit rate " << codecContext->bit_rate << "kb/s"
                << ", fps " << ((float)codecContext->framerate.num / (float)codecContext->framerate.den)
                << ", time base " << codecContext->time_base.num  << "/" << codecContext->time_base.den
                << std::endl;
    }

    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        cerr << "Error: Failed to open codec" << endl;
        avcodec_close(codecContext);
        avformat_close_input(&formatContext);
        return 1;
    }

    // Read frames from the video file and save them
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = NULL;
    packet.size = 0;
    int frameNumber = 0;

    while (av_read_frame(formatContext, &packet) >= 0) {
        if (packet.stream_index == videoStreamIndex) {
            AVFrame* frame = av_frame_alloc();
            if (!frame) {
                cerr << "Error: Failed to allocate frame" << endl;
                break;
            }

            ++counter2;
            // Print information about the packet
            // std::cout << "PACKET:" << counter2
            //     << " PTS: " << packet.pts
            //     << " DTS: " << packet.dts
            //     << " Size: " << packet.size
            //     << "     Keyframe: " << (packet.flags & AV_PKT_FLAG_KEY ? "I-PACKET" : "B(OR)P-PACKET")
            //     << std::endl;

            std::string file_path_compressed = "/home/phani/MMWorkSpace/VideoBasics/ExtractFrames/CompressedPackets/pgm-" + std::to_string(counter2) + "-" + (packet.flags & AV_PKT_FLAG_KEY ? "I" : "BP") + ".bin";
            std::ofstream output_stream(file_path_compressed, std::ios::binary);
            output_stream.write(reinterpret_cast<const char*>(packet.data), packet.size);

            int response = avcodec_send_packet(codecContext, &packet);
            if (response < 0) {
                cerr << "Error: Failed to send packet to codec" << endl;
                av_frame_free(&frame);
                break;
            }

            response = avcodec_receive_frame(codecContext, frame);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                av_frame_free(&frame);
                continue;
            } else if (response < 0) {
                cerr << "Error: Failed to receive frame from codec" << endl;
                av_frame_free(&frame);
                break;
            }

            time_t currentTime = time(nullptr);
            char* timeString = ctime(&currentTime);
            size_t length = strlen(timeString);
            if (length > 0 && timeString[length - 1] == '\n') {
                timeString[length - 1] = '\0';
            }
            std::string frame_info = "";
            if (frame->pict_type == AVPictureType::AV_PICTURE_TYPE_I) {
                frame_info = "I-FRAME";
            }
            else if (frame->pict_type == AVPictureType::AV_PICTURE_TYPE_P) {
                frame_info = "P-FRAME";
            }
            else if (frame->pict_type == AVPictureType::AV_PICTURE_TYPE_B) {
                frame_info = "B-FRAME";
            }
            else if (frame->pict_type == AVPictureType::AV_PICTURE_TYPE_S) {
                frame_info = "S-FRAME";
            }
            else if (frame->pict_type == AVPictureType::AV_PICTURE_TYPE_SI) {
                frame_info = "SI-FRAME";
            }
            else if (frame->pict_type == AVPictureType::AV_PICTURE_TYPE_SP) {
                frame_info = "SP-FRAME";
            }
            else if (frame->pict_type == AVPictureType::AV_PICTURE_TYPE_BI) {
                frame_info = "BI-FRAME";
            }
    
            //std::cout << timeString << ":" << frame->pts << ":" << frame->pkt_dts << ":" << frame->pkt_size << ":" << frame_info << std::endl;
            int frame_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, frame->width, frame->height, 1);
    
            // Print information about the frame
            // std::cout << "FRAME: " << counter2
            //     << " PTS: " << frame->pts
            //     << " DTS: " << frame->pkt_dts
            //     << " Size: " << frame_size
            //     << " Keyframe: " << frame_info << std::endl;
      
            ++counter3;

            if(counter3 == 301) {
                // Y value
                uint8_t Y = frame->data[0][10 * frame->linesize[0] + 10];
                // U value
                uint8_t U = frame->data[1][(10 / 2) * frame->linesize[1] + (10 / 2)];
                // V value
                uint8_t V = frame->data[2][(10 / 2) * frame->linesize[2] + (10 / 2)];
                printf("Frame 301: 10th row 10th column (Y:%d, U:%d, V:%d) ", Y, U, V);
            }


            std::string file_path_pgm = "/home/phani/MMWorkSpace/VideoBasics/ExtractFrames/rawFrames/pgm-" + std::to_string(counter3) + "-" + frame_info + ".ppm";
            save_frame_as_image(frame, file_path_pgm.c_str());

            //

            av_frame_free(&frame);
            frameNumber++;
        }

        av_packet_unref(&packet);
    }

    // Clean up
    avcodec_close(codecContext);
    avformat_close_input(&formatContext);

    return 0;
}

