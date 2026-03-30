#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/dict.h>

#define W 640
#define H 480
#define FPS 60.0
#define N (60 * 60)

double get_wall_time() {
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  return (double)time.tv_sec + (double)time.tv_nsec * 1e-9;
}

long get_file_size(const char *filename) {
  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    return -1; 
  }

  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  fclose(file);

  return size;
}

#define cnow (clock() * 1e-6)
#define rnow get_wall_time()

#define NCPU "8"
#define CRF "31"
#define PRESET "good"

int main(int argc, char **argv) {

  double rs = rnow;
  double cs = cnow;

  if (argc < 2) {
    printf("Usage: ./rand {output_file} {codec_name}\n");
    return 1;
  }

  char *codec_name = "libvpx";
  if (argc >= 3) {
    codec_name = argv[2];
  }

  const char *out_filename = argv[1];

  AVFormatContext *ofmt_ctx = NULL;
  AVCodecContext *codec_ctx = NULL;
  AVFrame *frame = NULL;
  AVPacket *pkt = NULL;

  avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
  
  const AVCodec *codec = avcodec_find_encoder_by_name(codec_name);
  if (!codec) {
      fprintf(stderr, "Fatal: Could not find encoder '%s'.\n", codec_name);
      fprintf(stderr, "Did you compile FFmpeg with this encoder enabled?\n");
      return 1;
  }
  
  AVStream *out_stream = avformat_new_stream(ofmt_ctx, codec);
  
  codec_ctx = avcodec_alloc_context3(codec);
  codec_ctx->width = W;
  codec_ctx->height = H;
  codec_ctx->time_base = (AVRational){1, FPS}; 
  codec_ctx->framerate = (AVRational){FPS, 1}; 
  codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P; 

  AVDictionary *vpx_opts = NULL;
  av_dict_set(&vpx_opts, "cpu-used", NCPU, 0);
  av_dict_set(&vpx_opts, "crf", CRF, 0);
  av_dict_set(&vpx_opts, "deadline", PRESET, 0);

  avcodec_open2(codec_ctx, codec, &vpx_opts);
  av_dict_free(&vpx_opts);
  avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);

  avio_open(&ofmt_ctx->pb, argv[1], AVIO_FLAG_WRITE);
  avformat_write_header(ofmt_ctx, NULL);

  frame = av_frame_alloc();
  frame->format = codec_ctx->pix_fmt;
  frame->width  = codec_ctx->width;
  frame->height = codec_ctx->height;
  av_frame_get_buffer(frame, 0); 

  pkt = av_packet_alloc();

  for (int i = 0; i < N; i++) {
    av_frame_make_writable(frame);

    for (int y = 0; y < codec_ctx->height; y++) {
      for (int x = 0; x < codec_ctx->width; x++) {
        frame->data[0][y * frame->linesize[0] + x] = rand() % 256;
      }
    }

    for (int y = 0; y < codec_ctx->height / 2; y++) {
      for (int x = 0; x < codec_ctx->width / 2; x++) {
        frame->data[1][y * frame->linesize[1] + x] = 128;
        frame->data[2][y * frame->linesize[2] + x] = 128;
      }
    }

    frame->pts = i;
    avcodec_send_frame(codec_ctx, frame);
    while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
      av_packet_rescale_ts(pkt, codec_ctx->time_base, out_stream->time_base);
      pkt->stream_index = out_stream->index;
      av_interleaved_write_frame(ofmt_ctx, pkt);
      av_packet_unref(pkt);
    }
  }

  avcodec_send_frame(codec_ctx, NULL);
  while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
    av_packet_rescale_ts(pkt, codec_ctx->time_base, out_stream->time_base);
    pkt->stream_index = out_stream->index;
    av_interleaved_write_frame(ofmt_ctx, pkt);
    av_packet_unref(pkt);
  }

  av_write_trailer(ofmt_ctx);

  av_frame_free(&frame);
  av_packet_free(&pkt);
  avcodec_free_context(&codec_ctx);
  avio_closep(&ofmt_ctx->pb);
  avformat_free_context(ofmt_ctx);

  double rdt = rnow - rs;
  double cdt = cnow - cs;

  printf("real time       : %.2f\n", rdt);
  printf("CPU time        : %.2f\n", cdt);
  printf("video duration  : %.2f\n", N/FPS);
  printf("temporal ratio  : %.2f\n", rdt * FPS / N);

  long compressed_size = get_file_size(argv[1]);

  printf("real size       : %ldB\n", H * W * 3 * N);
  printf("compressed size : %ldB\n", compressed_size);
  printf("statial ratio   : %.2f\n", (double) H * W * 3 * N / compressed_size);

  return 0;
}
