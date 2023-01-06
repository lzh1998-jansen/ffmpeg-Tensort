#include <stdio.h>
#include <string>
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

extern "C"
{

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"

};

using namespace std;

int main(int argc, char *argv[]) {
    const char *path = "rtsp://admin:poiu1234@124.193.193.43:15554/h264/ch1/sub/av_stream";
    AVFormatContext *ifmt_ctx = NULL;
    int ret;
    AVDictionary *avdic = NULL;
    char option_key[] = "rtsp_transport";
    char option_value[] = "tcp";
    av_dict_set(&avdic, option_key, option_value, 0);
    char option_key2[] = "max_delay";
    char option_value2[] = "5000000";
    av_dict_set(&avdic, option_key2, option_value2, 0);

    av_register_all();
    avformat_network_init();

    if ((ret = avformat_open_input(&ifmt_ctx, path, 0, &avdic)) < 0) {
        printf("Could not open input file.\n");
        return -1;
    }

    printf("avformat_open_input successfully\n");
    if ((ret = avformat_find_stream_info(ifmt_ctx,0)) < 0) {
        printf("Failed to retrieve input stream information\n");
        return -1;
    }
    printf("avformat_find_stream_info successfully\n");
    int time = ifmt_ctx->duration;
    int mbittime = (time / 100000) / 60;
    int mminttime = (time / 100000) % 60;
    printf("video time: %d'm %d's\n", mbittime, mminttime);
    av_dump_format(ifmt_ctx, 0, path, 0);
    int videoindex = -1;
    for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoindex = i;
            break;
        }
    }
    if (videoindex == -1) {
        printf("don't find video stream\n");
        return -1;
    }


    AVCodecParameters *codecParameters = ifmt_ctx->streams[videoindex]->codecpar;
    printf("video width %d\n", codecParameters->width);
    printf("video height %d\n", codecParameters->height);
    AVCodec *pCodec = avcodec_find_decoder(codecParameters->codec_id);
    AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);
    //编码器
    ret = avcodec_open2(pCodecCtx, pCodec, NULL);
    if (ret < 0) {
        printf("Could not open codec.\n");
        return -1;
    }
    AVFrame *picture = av_frame_alloc();
    picture->width = codecParameters->width;
    picture->height = codecParameters->height;
    picture->format = AV_PIX_FMT_YUV420P;
    ret = av_frame_get_buffer(picture, 1);
    if (ret < 0) {
        printf("av_frame_get_buffer error\n");
        return -1;
    }
    printf("picture->linesize[0] %d\n", picture->linesize[0]);

    AVFrame *pFrame = av_frame_alloc();
    pFrame->width = codecParameters->width;
    pFrame->height = codecParameters->height;
    pFrame->format = AV_PIX_FMT_YUV420P;
    ret = av_frame_get_buffer(pFrame, 1);
    if (ret < 0) {
        printf("av_frame_get_buffer error\n");
        return -1;
    }

    AVFrame *pFrameRGB = av_frame_alloc();
    pFrameRGB->width = codecParameters->width;
    pFrameRGB->height = codecParameters->height;
    pFrameRGB->format = AV_PIX_FMT_RGB24;
    ret = av_frame_get_buffer(pFrameRGB, 1);
    if (ret < 0) {
        printf("av_frame_get_buffer error\n");
        return -1;
    }


    int picture_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, codecParameters->width, codecParameters->height,
                                                1);//计算这个格式的图片，需要多少字节来存储
    uint8_t *out_buff = (uint8_t *) av_malloc(picture_size * sizeof(uint8_t));
    av_image_fill_arrays(picture->data, picture->linesize, out_buff, AV_PIX_FMT_YUV420P, codecParameters->width,
                         codecParameters->height, 1);
    //这个函数 是缓存转换格式，可以不用 以为上面已经设置了AV_PIX_FMT_YUV420P
    SwsContext *img_convert_ctx = sws_getContext(codecParameters->width, codecParameters->height, AV_PIX_FMT_YUV420P,
                                                 codecParameters->width, codecParameters->height, AV_PIX_FMT_RGB24, 4,
                                                 NULL, NULL, NULL);
    AVPacket *packet = (AVPacket *) av_malloc(sizeof(AVPacket));


    while (av_read_frame(ifmt_ctx, packet) >= 0) {
        if (packet->stream_index == videoindex) {
            ret = avcodec_send_packet(pCodecCtx, packet);
            if (ret < 0) {
                printf("avcodec_send_packet error\n");
                continue;
            }
            av_packet_unref(packet);
            int got_picture = avcodec_receive_frame(pCodecCtx, pFrame);
            if (got_picture < 0) {
                printf("avcodec_receive_frame error\n");
                continue;
            }

            sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0,
                      codecParameters->height,
                      pFrameRGB->data, pFrameRGB->linesize);


            cv::Mat RGBimg(cv::Size(codecParameters->width, codecParameters->height), CV_8UC3);
            RGBimg.data = (unsigned char *) pFrameRGB->data[0];
//            cv::imwrite("./rgb.png",RGBimg);
            cv::Mat BGRimg;
            cv::cvtColor(RGBimg, BGRimg, cv::COLOR_RGB2BGR);
//            cv::imshow("demo", BGRimg);
//            cv::waitKey(1);
//            cv::imwrite("./bgr.png",BGRimg);
//            cv::destroyAllWindows();
        }
    }
    av_frame_free(&picture);
    av_frame_free(&pFrame);
    av_frame_free(&pFrameRGB);
    avformat_free_context(ifmt_ctx);
    return 0;
}
