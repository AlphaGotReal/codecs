#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/dict.h>

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

#define PRESET "medium"
#define CRF "23"
#define GOP "10"
#define WIDTH  640
#define HEIGHT 480
#define DEFAULT_FPS 30
#define NFRAMES 300

int main(int argc, char **argv) {

  double rs = rnow;
  double cs = cnow;

  if (argc < 2) {
    printf("Usage: ./random {output_file} [width] [height] [fps] [frames] [codec_name] [preset] [crf] [GOP]\n");
    return 1;
  }

  const char *out_filename = argv[1];

  int W = WIDTH;
  if (argc >= 3) {
    W = atoi(argv[2]);
    if (W <= 0) W = WIDTH;
  }

  int H = HEIGHT;
  if (argc >= 4) {
    H = atoi(argv[3]);
    if (H <= 0) H = HEIGHT;
  }

  double FPS = DEFAULT_FPS;
  if (argc >= 5) {
    FPS = atof(argv[4]);
    if (FPS <= 0 || FPS > 1000) FPS = 30.0;
  }

  long N = NFRAMES;
  if (argc >= 6) {
    N = atol(argv[5]);
    if (N <= 0) N = 300;
  }

  char *codec_name = "libx264";
  if (argc >= 7) {
    codec_name = argv[6];
  }

  const char *preset = PRESET;
  if (argc >= 8) {
    preset = argv[7];
  }

  const char *crf = CRF;
  if (argc >= 9) {
    crf = argv[8];
  }

  const char *gop = GOP;
  if (argc >= 10) {
    gop = argv[9];
  } 

  printf("Output: %s (%dx%d, %.2f fps, %ld frames, codec=%s)\n", out_filename, W, H, FPS, N, codec_name);

  AVFormatContext *ofmt_ctx = NULL;
  AVCodecContext *codec_ctx = NULL;
  AVFrame *frame = NULL;
  AVPacket *pkt = NULL;

  avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);

  const AVCodec *enc_codec = avcodec_find_encoder_by_name(codec_name);
  if (!enc_codec) {
    fprintf(stderr, "Fatal: Could not find encoder '%s'.\n", codec_name);
    fprintf(stderr, "Did you compile FFmpeg with this encoder enabled?\n");
    return 1;
  }

  AVStream *out_stream = avformat_new_stream(ofmt_ctx, enc_codec);

  codec_ctx = avcodec_alloc_context3(enc_codec);
  codec_ctx->width = W;
  codec_ctx->height = H;
  codec_ctx->time_base = (AVRational){1, FPS};
  codec_ctx->framerate = (AVRational){FPS, 1};
  codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

  AVDictionary *opts = NULL;
  av_dict_set(&opts, "preset", preset, 0);
  av_dict_set(&opts, "crf", crf, 0);
  av_dict_set(&opts, "g", gop, 0);

  if (avcodec_open2(codec_ctx, enc_codec, &opts) < 0) {
    fprintf(stderr, "Error: Could not open encoder\n");
    return 1;
  }
  av_dict_free(&opts);

  avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
  out_stream->time_base = codec_ctx->time_base;

  if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
    fprintf(stderr, "Error: Could not open output file '%s'\n", out_filename);
    return 1;
  }
  if (avformat_write_header(ofmt_ctx, NULL) < 0) {
    fprintf(stderr, "Error: Could not write header\n");
    return 1;
  }

  frame = av_frame_alloc();
  frame->format = codec_ctx->pix_fmt;
  frame->width  = codec_ctx->width;
  frame->height = codec_ctx->height;
  av_frame_get_buffer(frame, 0);

  pkt = av_packet_alloc();

  for (int64_t pts = 0; pts < N; pts++) {
    av_frame_make_writable(frame);

    for (int y = 0; y < H; y++)
      for (int x = 0; x < W; x++)
        frame->data[0][y * frame->linesize[0] + x] = rand() % 256;

    for (int y = 0; y < H / 2; y++)
      for (int x = 0; x < W / 2; x++)
        frame->data[1][y * frame->linesize[1] + x] = rand() % 256;

    for (int y = 0; y < H / 2; y++)
      for (int x = 0; x < W / 2; x++)
        frame->data[2][y * frame->linesize[2] + x] = rand() % 256;

    frame->pts = pts;

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

  long compressed_size = get_file_size(out_filename);
  long uncompressed_size = (long)(H * W * 1.5 * N);

  printf("original size   : %ldB\n", uncompressed_size);
  printf("compressed size : %ldB\n", compressed_size);
  printf("compression ratio: %.2f\n", (double)uncompressed_size / compressed_size);

  return 0;
}
