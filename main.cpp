#include <iostream>
//#include <opencv2/opencv.hpp>
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/time.h"
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
}
#include "hello.h"
#include <unistd.h>
//using namespace cv;
using namespace std;

//int main() {
//    hello("asd");
//    std::cout << "Hello, World!" << std::endl;
////    Mat image;
////    image= imread("F:\\leecode\\cv\\stream_analysis\\clion_test\\1.png");
////    namedWindow("显示图片", WINDOW_AUTOSIZE);
////    imshow("显示图片", image);
////    waitKey(0);
////    destroyAllWindows();
//    char in_filename[128] = {1}, out_filename[128] = {0};
//    cout<<in_filename<<endl;
//    cout<<"out-filename"<<out_filename<<endl;
//
//    return 0;
//}
#define USAGE   "rtsp2x -i <rtsp url> -t [avi | flv | mp4] -n <number of frames you want to save>"
#define OPTS    "i:t:n:h"
static void print_usage()
{
    printf("Usage: %s\n", USAGE);
    return;
}
int main(int argc,char **argv)
{
    cout<<"argc"<<argc<<endl;
    cout<<"argv"<<argv<<endl;
    cout<<"test"<<endl;
    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ifmt_ctx = NULL;
    AVFormatContext *ofmt_ctx = NULL;
    AVPacket pkt;
    char in_filename[128] = {0}, out_filename[128] = {0};
    int ret,i;
    int video_index=-1;
    int frame_index=0;
    int I_received = 0;
    int opt, frames_count = -1;
    while ((opt = getopt(argc, argv, OPTS)) != -1) {
        switch (opt) {
            case 'i':
                strcpy(in_filename, optarg);
                break;
            case 't':
                if (strcmp(optarg, "avi") == 0)
                    strcpy(out_filename, "receive.avi");
                else if (strcmp(optarg, "flv") == 0)
                    strcpy(out_filename, "receive.flv");
                else if (strcmp(optarg, "mp4") == 0)
                    strcpy(out_filename, "receive.mp4");
                else {
                    return -1;
                }
                print_usage();
                break;
            case 'n':
                frames_count = atoi(optarg);
                if (frames_count < 0) {
                    print_usage();
                    return -1;
                }
                printf("frames_count = %d\n", frames_count);
                break;
            case 'h':
            default:
                print_usage();
                return -1;
        }
    }

    if (strlen(in_filename) == 0 || strlen(out_filename) == 0 || frames_count < 0) {
        print_usage();
        return -1;
    }

    av_register_all();
    avformat_network_init();

    //使用TCP连接打开RTSP，设置最大延迟时间
    AVDictionary *avdic=NULL;
    char option_key[]="rtsp_transport";
    char option_value[]="tcp";
    av_dict_set(&avdic,option_key,option_value,0);
    char option_key2[]="max_delay";
    char option_value2[]="5000000";
    av_dict_set(&avdic,option_key2,option_value2,0);
    //打开输入流
    if((ret=avformat_open_input(&ifmt_ctx,in_filename,0,&avdic))<0)
    {
        printf("Could not open input file.\n");
        goto end;
    }
    if((ret=avformat_find_stream_info(ifmt_ctx,0))<0)
    {
        printf("Failed to retrieve input stream information\n");
        goto end;
    }
    //nb_streams代表有几路流，一般是2路：即音频和视频，顺序不一定
    for(i=0;i<ifmt_ctx->nb_streams;i++){

        if(ifmt_ctx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
        {
            //这一路是视频流，标记一下，以后取视频流都从ifmt_ctx->streams[video_index]取
            video_index=i;
            break;
        }
    }

    av_dump_format(ifmt_ctx,0,in_filename,0);

    //打开输出流
    avformat_alloc_output_context2(&ofmt_ctx,NULL,NULL,out_filename);

    if(!ofmt_ctx)
    {
        printf("Could not create output context\n");
        ret=AVERROR_UNKNOWN;
        goto end;
    }

    ofmt = ofmt_ctx->oformat;
    for(i=0;i<ifmt_ctx->nb_streams;i++)
    {    //根据输入流创建输出流
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx,in_stream->codec->codec);
        if(!out_stream)
        {
            printf("Failed allocating output stream.\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }

        //将输出流的编码信息复制到输入流
        ret = avcodec_copy_context(out_stream->codec,in_stream->codec);
        if(ret<0)
        {
            printf("Failed to copy context from input to output stream codec context\n");
            goto end;
        }
        out_stream->codec->codec_tag = 0;

        if(ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    }

    //Dump format--------------------
    av_dump_format(ofmt_ctx,0,out_filename,1);
    //打开输出文件
    if(!(ofmt->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ofmt_ctx->pb,out_filename,AVIO_FLAG_WRITE);
        if(ret<0)
        {
            printf("Could not open output URL '%s'",out_filename);
            goto end;
        }
    }

    //写文件头到输出文件
    ret = avformat_write_header(ofmt_ctx,NULL);
    if(ret < 0)
    {
        printf("Error occured when opening output URL\n");
        goto end;
    }


    //while循环中持续获取数据包，不管音频视频都存入文件
    while(1)
    {
        AVStream *in_stream,*out_stream;
        //从输入流获取一个数据包
        ret = av_read_frame(ifmt_ctx,&pkt);
        if(ret<0)
            break;

        in_stream = ifmt_ctx->streams[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];
        //copy packet
        //转换 PTS/DTS 时序
        pkt.pts = av_rescale_q_rnd(pkt.pts,in_stream->time_base,out_stream->time_base,(enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        //printf("pts %d dts %d base %d\n",pkt.pts,pkt.dts, in_stream->time_base);
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;

        //此while循环中并非所有packet都是视频帧，当收到视频帧时记录一下，仅此而已
        if(pkt.stream_index==video_index)
        {
            if ((pkt.flags & AV_PKT_FLAG_KEY) && (I_received == 0))
                I_received = 1;
            if (I_received == 0)
                continue;

            printf("Receive %8d video frames from input URL\n",frame_index);
            frame_index++;
        } else {
            continue;
        }

        if (frame_index == frames_count)
            break;

        //将包数据写入到文件。
        ret = av_interleaved_write_frame(ofmt_ctx,&pkt);
        if(ret < 0)
        {
            /**
            当网络有问题时，容易出现到达包的先后不一致，pts时序混乱会导致
            av_interleaved_write_frame函数报 -22 错误。暂时先丢弃这些迟来的帧吧
            若所大部分包都没有pts时序，那就要看情况自己补上时序（比如较前一帧时序+1）再写入。
            */
            if(ret==-22){
                continue;
            }else{
                printf("Error muxing packet.error code %d\n" , ret);
                break;
            }

        }

        //av_free_packet(&pkt); //此句在新版本中已deprecated 由av_packet_unref代替
        av_packet_unref(&pkt);
    }


    //写文件尾
    av_write_trailer(ofmt_ctx);

    end:

    av_dict_free(&avdic);
    avformat_close_input(&ifmt_ctx);
    //Close input
    if(ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_close(ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    if(ret<0 && ret != AVERROR_EOF)
    {
        printf("Error occured.\n");
        return -1;
    }
    return 0;

}