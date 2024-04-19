#include <string>
#include <iostream>
#include <vector>
#include <array>
#include <algorithm>

extern "C" {
    #include "libavformat/avformat.h"
    #include "libavformat/avio.h"
    #include "libavcodec/avcodec.h"
    #include "libavutil/audio_fifo.h"
    #include "libavutil/avassert.h"
    #include "libavutil/avstring.h"
    #include "libavutil/frame.h"
    #include "libavutil/opt.h"
    #include "libswresample/swresample.h"
    #include "libavutil/pixdesc.h"
    #include "libswscale/swscale.h"
    #include "libavutil/imgutils.h"
    #include <libavutil/mem.h>
}

//General
std::string output_file = "encoded_video_from_one_image.mp4";
int64_t pts = 0;
const int width = 640;
const int height = 360;

//FFMPEG intialization
AVFormatContext *output_format_context = nullptr;
AVCodecContext  *output_codec_context_video = nullptr;
AVCodec  *output_codec_video = nullptr;
AVStream *stream   = NULL;
const enum AVPixelFormat out_pix_fmt = AV_PIX_FMT_YUV420P;


//RGBtoYUV420p
std::vector<std::array<int, 4>> out_linesize_vector = {};
std::vector<std::array<uint8_t*, 4>> out_planes_vector = {};

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

int ffmpeg_initialization()
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

    av_opt_set(output_codec_context_video->priv_data, "preset", "slow", 0);
    output_codec_context_video->height = height;
    output_codec_context_video->width = width;
    output_codec_context_video->pix_fmt = out_pix_fmt;

    output_codec_context_video->bit_rate = 400000;

    output_codec_context_video->time_base = AVRational{.num = 1, .den = 25};
    output_codec_context_video->max_b_frames = 1;
    output_codec_context_video->gop_size = 10;

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

int convertRGBtoYUV420p(uint8_t* out_planes[4], int out_linesize[4], const std::string& image_name)
{
    unsigned char*rgb_in = new uint8_t[width * height * 3];

    FILE* f = fopen(image_name.c_str(), "rb"); 
    fread(rgb_in, 1, width * height * 3, f);
    fclose(f);

    int sts = av_image_alloc(out_planes,    //uint8_t * pointers[4], 
                             out_linesize,  //int linesizes[4], 
                             width,         //int w, 
                             height,        //int h, 
                             out_pix_fmt,   //enum AVPixelFormat pix_fmt, 
                             32);           //int align);   //Align to 32 bytes address may result faster execution time compared to 1 byte aligenment.

    if (sts < 0)
    {
        printf("Error: av_image_alloc response = %d\n", sts);
        return -1;
    }

    struct SwsContext* sws_context = nullptr;
    sws_context = sws_getCachedContext(sws_context,         //struct SwsContext *context,
                                       width,               //int srcW,
                                       height,              //int srcH,
                                       AV_PIX_FMT_RGB24,    //enum AVPixelFormat srcFormat,
                                       width,               //int dstW,
                                       height,              //int dstH,
                                       out_pix_fmt,         //enum AVPixelFormat dstFormat,
                                       SWS_BILINEAR,        //int flags,
                                       nullptr,             //SwsFilter *srcFilter,
                                       nullptr,             //SwsFilter *dstFilter,
                                       nullptr);            //const double *param);

    if (sws_context == nullptr)
    {
        printf("Error: sws_getCachedContext returned nullptr\n");
        return -1;
    }

    //Apply color conversion
    ////////////////////////////////////////////////////////////////////////////
    const int in_linesize[1] = { 3 * width }; // RGB stride
    const uint8_t* in_planes[1] = { rgb_in };

    int response = sws_scale(sws_context,   //struct SwsContext *c, 
                             in_planes,     //const uint8_t *const srcSlice[],
                             in_linesize,   //const int srcStride[], 
                             0,             //int srcSliceY, 
                             height,        //int srcSliceH,
                             out_planes,    //uint8_t *const dst[], 
                             out_linesize); //const int dstStride[]);


    if (response < 0)
    {
        printf("Error: sws_scale response = %d\n", response);
        return -1;
    }

    std::array<uint8_t*, 4> current_plane;
    for (int j = 0; j < 4; ++j) {
            current_plane[j] = out_planes[j];
    }
    out_planes_vector.push_back(current_plane);

    std::array<int, 4> current_outlinesize;
    for (int j = 0; j < 4; ++j) {
            current_outlinesize[j] = out_linesize[j];
    }
    out_linesize_vector.push_back(current_outlinesize);

    //Write YUV420p output image to binary file (for testing)
    size_t lastSlashPos = image_name.find_last_of("/");
    std::string filenameWithExtension = image_name.substr(lastSlashPos + 1);
    size_t lastDotPos = filenameWithExtension.find_last_of(".");
    std::string newFilename = filenameWithExtension.substr(0, lastDotPos) + ".bin";


    f = fopen(newFilename.c_str(), "wb");
    fwrite(current_plane[0], 1, width * height, f);
    fwrite(current_plane[1], 1, width * height / 4, f);
    fwrite(current_plane[2], 1, width * height / 4, f);
    fclose(f);

    sws_freeContext(sws_context);
    delete[] rgb_in;
    av_free(out_planes);
    return 0;
}

int DumpYUV420PToAVFrame(AVFrame **frame, uint8_t* out_planes[4], int out_linesize[4])
{
   *frame = av_frame_alloc();
   if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        return -1;
    }
    
    (*frame)->format = out_pix_fmt;
    (*frame)->width  = width;
    (*frame)->height = height;
    
    // Allocate the buffers for the frame data
    if (av_frame_get_buffer(*frame, 32) < 0) { // Use the same alignment as av_image_alloc
        fprintf(stderr, "Could not allocate frame data.\n");
        av_frame_free(&(*frame));
        return -1;
    }

    // Copy YUV data from out_planes to the frame
    // Y plane
    av_image_copy_plane((*frame)->data[0], (*frame)->linesize[0], out_planes[0], out_linesize[0], width, height);
    // U plane
    av_image_copy_plane((*frame)->data[1], (*frame)->linesize[1], out_planes[1], out_linesize[1], width / 2, height / 2);
    // V plane
    av_image_copy_plane((*frame)->data[2], (*frame)->linesize[2], out_planes[2], out_linesize[2], width / 2, height / 2);
    
    // At this point, 'frame' contains the YUV data and can be processed further or encoded using an encoder.
    //save_frame_as_image(frame, "sample.pgm");
    return 0;
}

int DumpYUV420PToAVFrame(AVFrame **frame, int image_to_show) {
    uint8_t* out_planes_420p[4] = { nullptr, nullptr, nullptr, nullptr };
    int out_linesize_420p[4] = {0, 0, 0, 0};

    std::copy(out_planes_vector[image_to_show].begin(), out_planes_vector[image_to_show].end(), out_planes_420p);
    std::copy(out_linesize_vector[image_to_show].begin(), out_linesize_vector[image_to_show].end(), out_linesize_420p);

    int response = DumpYUV420PToAVFrame(frame, out_planes_420p, out_linesize_420p);
    return response;
}


int main()
{
    if(ffmpeg_initialization() < 0)
    {
        std::cout << "Error in Intialization" << std::endl;
        return -1;
    }

    std::vector<std::string> filenames = {
       "/home/phani/MMWorkSpace/VideoBasics/ExtractFrames/rawFrames/pgm-397-I-FRAME.ppm",
       "/home/phani/MMWorkSpace/VideoBasics/ExtractFrames/rawFrames/pgm-301-P-FRAME.ppm",
       "/home/phani/MMWorkSpace/VideoBasics/ExtractFrames/rawFrames/pgm-663-P-FRAME.ppm",
       "/home/phani/MMWorkSpace/VideoBasics/ExtractFrames/rawFrames/pgm-593-P-FRAME.ppm"
    };

    int num_images = filenames.size();

    for(int i = 0; i < num_images; ++i)
    {
        int out_linesize_image[4] = {0, 0, 0, 0};
        uint8_t* out_planes_image[4] =  { nullptr, nullptr, nullptr, nullptr };

        if(convertRGBtoYUV420p(out_planes_image, out_linesize_image, filenames[i]) < 0)
        {
            std::cout << "Error in Converting RGB to YUV" << std::endl;
            return -1;
        }
    }

    for(int i = 0; i < 143; ++i) {
        AVFrame* frame = nullptr;
        int response = 0;
        if(i <= 43) {
            response = DumpYUV420PToAVFrame(&frame, 0);
            if(response < 0)
            {
                std::cout << "Error on Dumping " << filenames[0] << std::endl;
                return -1;
            }
        }
        else if (i >43 && i <= 68){
            response = DumpYUV420PToAVFrame(&frame, 1);
            if(response < 0)
            {
                std::cout << "Error on Dumping " << filenames[1] << std::endl;
                return -1;
            }
        }
        else if(i > 68 && i <= 93) {
            response = DumpYUV420PToAVFrame(&frame, 2);
            if(response < 0)
            {
                std::cout << "Error on Dumping " << filenames[2] << std::endl;
                return -1;
            }
        }
        else if(i > 93 && i <= 118) {
            response = DumpYUV420PToAVFrame(&frame, 3);
            if(response < 0)
            {
                std::cout << "Error on Dumping " << filenames[3] << std::endl;
                return -1;
            }
        }
        else {
            response = DumpYUV420PToAVFrame(&frame, 0);
            if(response < 0)
            {
                std::cout << "Error on Dumping " << filenames[0] << std::endl;
                return -1;
            }
        }
        
        frame->pts = ++pts;

        response = avcodec_send_frame(output_codec_context_video, frame);
        if (response < 0 && response != AVERROR(EAGAIN) && response != AVERROR_EOF) {
            std::cout << "Error sending a frame for encoding\n";
            return -1;
        }
      
        while (response >= 0) {
            AVPacket *output_packet = av_packet_alloc();
    
            if (!output_packet) {
              std::cout << "could not allocate memory for output packet" << std::endl;
              return -1;
            }  
              
            response = avcodec_receive_packet(output_codec_context_video, output_packet);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
              break;
            } else if (response < 0) {
              std::cout << "Error while receiving packet from encoder:" << std::endl; //av_err2str(response) << endl;
              return -1;
            }

            // Print information about the packet
            std::cout << "PACKET:" << i 
            << " PTS: " << output_packet->pts
            << " DTS: " << output_packet->dts
            << " Size: " << output_packet->size
            << "     Keyframe: " << (output_packet->flags & AV_PKT_FLAG_KEY ? "I-PACKET" : "B(OR)P-PACKET")
            << std::endl;

        
            output_packet->stream_index = stream->index;
            av_packet_rescale_ts(output_packet, output_codec_context_video->time_base, stream->time_base);
            response = av_interleaved_write_frame(output_format_context, output_packet);
            if(response != 0) {
              std::cout << "Error " << response <<  "receiving packet from decoder:" << std::endl; //av_err2str(response) << endl;
              return -1;
            }
            else{
                std::cout << "Packet:" << i << " written" << std::endl;
                std::cout << "========================================================" << std::endl;
            }
           av_packet_unref(output_packet);
           av_packet_free(&output_packet);
        }
    
        // After you're done with the frame, don't forget to free it
        av_frame_free(&frame);
    }

    int error;
    if ((error = av_write_trailer(output_format_context)) < 0) {
        std::cout << "Could not write output file trailer." << std::endl;
        return error;
    }

    if (output_format_context) {
        avio_closep(&output_format_context->pb);
        avformat_free_context(output_format_context);
    } 
    if (output_codec_context_video)
        avcodec_free_context(&output_codec_context_video);

    return 0;
}
