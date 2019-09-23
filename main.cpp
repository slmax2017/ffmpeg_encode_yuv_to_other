
#include <iostream>
using namespace std;

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/mem.h"    
#include "libavutil/imgutils.h"
#include "libavformat/avformat.h"
}

#define IN_FILE_NAME  "cuc_ieschool_640x360_yuv420p.yuv"
#define OUT_FILE_NAME "cuc_ieschool_640x360_yuv420p.flv"

#define  ptr_check(x) \
            do {\
                if (!x){\
                    printf("operator fail"); \
                    return -1; \
                }\
            }while(0) 

#define ffmpeg_handle(x) \
            if ((x) < 0) {\
                printf("ffmpeg operator fail"); \
                return -1; \
            }

int main()
{
    FILE *in_fp = fopen(IN_FILE_NAME, "rb");
    FILE *out_fp = fopen(OUT_FILE_NAME, "wb");

    AVFormatContext *out_fmt_ctx = NULL;
    ffmpeg_handle(avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, OUT_FILE_NAME));

    AVStream *vedio_stream = avformat_new_stream(out_fmt_ctx, NULL);
    ptr_check(vedio_stream);
    vedio_stream->time_base.den = 20;
    vedio_stream->time_base.num = 1;

    AVCodecContext *codeCtx = NULL;
    AVDictionary *opts = NULL;
    codeCtx = vedio_stream->codec;
    codeCtx->width = 640;
    codeCtx->height = 360;
    codeCtx->codec_id = out_fmt_ctx->oformat->video_codec;
    codeCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    codeCtx->time_base = vedio_stream->time_base;
    codeCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    codeCtx->bit_rate = 400000;
    if (codeCtx->codec_id == AV_CODEC_ID_H264) {
        codeCtx->qmin = 40;
        codeCtx->qmax = 51;
        codeCtx->qcompress = 0.6;
        av_dict_set(&opts, "preset", "medium", 0);
        av_dict_set(&opts, "tune", "film", 0);
        av_dict_set(&opts, "profile", "main", 0);
    }

    //打开文件.写入文件头.文件尾需要
    ffmpeg_handle(avio_open(&out_fmt_ctx->pb, OUT_FILE_NAME, AVIO_FLAG_READ_WRITE));

    //查找编码器
    codeCtx->codec = avcodec_find_encoder(codeCtx->codec_id);
    ptr_check(codeCtx->codec);

    //打开编码器
    ffmpeg_handle(avcodec_open2(codeCtx, codeCtx->codec, &opts));

    AVFrame *frame = (AVFrame*)av_frame_alloc();
    frame->height = codeCtx->height;
    frame->width = codeCtx->width;
    frame->format = codeCtx->pix_fmt;

    int iSize = av_image_get_buffer_size(codeCtx->pix_fmt, frame->width, frame->height, 1);
    unsigned char *pic_buf = (unsigned char*)av_malloc(iSize);
    ffmpeg_handle(av_image_fill_arrays(frame->data, frame->linesize, pic_buf, codeCtx->pix_fmt, frame->width, frame->height, 1));

    //写文件头,之后尝试不用fmtCtx.看看后面能正常运行不
    avformat_write_header(out_fmt_ctx, NULL);

    AVPacket *packet = av_packet_alloc();
    ptr_check(packet);

    int y = frame->height * frame->width;
    int u = y / 4;
    int v = y / 4;
    int ret = -1;
    int index = 0;
    int i = 0;
    //循环每一次读取YUV,设置在frame->data中,可以一次读完,也可以分别读,先尝试分别读,y,u,v
    while (1) {

        //y
        ret = fread(frame->data[0], 1, y, in_fp);
        if (ret != y) break;
        //u
        ret = fread(frame->data[1], 1, u, in_fp);
        if (ret != u) break;
        //v
        ret = fread(frame->data[2], 1, v, in_fp);
        if (ret != v) break;

        frame->pts = i++;
        ret = (avcodec_send_frame(codeCtx, frame));

        while (ret >= 0) {

            ret = avcodec_receive_packet(codeCtx, packet);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;

            av_packet_rescale_ts(packet, codeCtx->time_base, vedio_stream->time_base);
            printf("Write packet frame = %d (size=%5d), dts = %lld, pts = %lld\n", ++index, packet->size, packet->dts, packet->pts);
            ffmpeg_handle(av_write_frame(out_fmt_ctx, packet));
            av_packet_unref(packet);

        }

    }

    ret = avcodec_send_frame(codeCtx, NULL);

    while (ret >= 0) {
        ret = avcodec_receive_packet(codeCtx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;

        //av_packet_rescale_ts(packet, codeCtx->time_base, vedio_stream->time_base);
        printf("Write packet frame = %d (size=%5d), dts = %lld, pts = %lld\n", ++index, packet->size, packet->dts, packet->pts);
        ffmpeg_handle(av_write_frame(out_fmt_ctx, packet));
        av_packet_unref(packet);
    }


    av_write_trailer(out_fmt_ctx);

    fclose(in_fp);
    fclose(out_fp);
    av_free(pic_buf);
    av_frame_free(&frame);
    av_packet_free(&packet);
    avformat_free_context(out_fmt_ctx);
    cin.get();
    return 0;
}