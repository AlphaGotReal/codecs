#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/dict.h>
#include <libswscale/swscale.h>
#include <libavutil/hwcontext.h>

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

#define DEFAULT_QUALITY "23"
#define DEFAULT_ASYNC_DEPTH "4"

int main(int argc, char **argv) {

  double rs = rnow;
  double cs = cnow;

  if (argc < 3) {
    printf("Usage: ./qsv {input_file} {output_file} [codec_name] [global_quality] [async_depth]\n");
    printf("  codec defaults to h264_qsv, also accepts hevc_qsv, av1_qsv, vp9_qsv, mpeg2_qsv, ...\n");
    return 1;
  }

  const char *in_filename = argv[1];
  const char *out_filename = argv[2];

  char *codec_name = "h264_qsv";
  if (argc >= 4) {
    codec_name = argv[3];
  }

  const char *quality = DEFAULT_QUALITY;
  if (argc >= 5) {
    quality = argv[4];
  }

  const char *async_depth = DEFAULT_ASYNC_DEPTH;
  if (argc >= 6) {
    async_depth = argv[5];
  }

  AVFormatContext *ifmt_ctx = NULL;
  if (avformat_open_input(&ifmt_ctx, in_filename, NULL, NULL) < 0) {
    fprintf(stderr, "Error: Could not open input file '%s'\n", in_filename);
    return 1;
  }

  if (avformat_find_stream_info(ifmt_ctx, NULL) < 0) {
    fprintf(stderr, "Error: Could not find stream info\n");
    return 1;
  }

  int video_stream_idx = -1;
  for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
    if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_idx = i;
      break;
    }
  }

  if (video_stream_idx == -1) {
    fprintf(stderr, "Error: Could not find video stream in input file\n");
    return 1;
  }

  AVStream *in_stream = ifmt_ctx->streams[video_stream_idx];
  AVCodecParameters *in_codecpar = in_stream->codecpar;

  int W = in_codecpar->width;
  int H = in_codecpar->height;

  AVRational fps = av_guess_frame_rate(ifmt_ctx, in_stream, NULL);
  double FPS = av_q2d(fps);
  if (FPS <= 0 || FPS > 1000) {
    FPS = 30.0;
  }

  int64_t duration = in_stream->duration;
  if (duration <= 0) {
    duration = ifmt_ctx->duration;
  }
  long N = (long)(av_q2d(in_stream->time_base) * duration * FPS);
  if (N <= 0) {
    N = 300;
  }

  printf("Input: %s (%dx%d, %.2f fps, %ld frames)\n", in_filename, W, H, FPS, N);

  const AVCodec *dec_codec = avcodec_find_decoder(in_codecpar->codec_id);
  if (!dec_codec) {
    fprintf(stderr, "Fatal: Could not find decoder\n");
    return 1;
  }

  AVCodecContext *dec_ctx = avcodec_alloc_context3(dec_codec);
  avcodec_parameters_to_context(dec_ctx, in_codecpar);
  avcodec_open2(dec_ctx, dec_codec, NULL);

  AVFormatContext *ofmt_ctx = NULL;
  AVCodecContext *codec_ctx = NULL;
  AVFrame *frame = NULL;
  AVPacket *pkt = NULL;

  avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
  
  const AVCodec *enc_codec = avcodec_find_encoder_by_name(codec_name);
  if (!enc_codec) {
    fprintf(stderr, "Fatal: Could not find encoder '%s'.\n", codec_name);
    fprintf(stderr, "Did you compile FFmpeg with QSV support enabled?\n");
    return 1;
  }

  AVBufferRef *vaapi_device_ctx = NULL;
  if (av_hwdevice_ctx_create(&vaapi_device_ctx, AV_HWDEVICE_TYPE_VAAPI,
                             "/dev/dri/renderD128", NULL, 0) < 0) {
    fprintf(stderr, "Fatal: Could not create VAAPI device context\n");
    return 1;
  }

  AVBufferRef *hw_device_ctx = NULL;
  int derive_ret = av_hwdevice_ctx_create_derived(&hw_device_ctx,
      AV_HWDEVICE_TYPE_QSV, vaapi_device_ctx, 0);
  av_buffer_unref(&vaapi_device_ctx);

  AVStream *out_stream = avformat_new_stream(ofmt_ctx, enc_codec);

  codec_ctx = avcodec_alloc_context3(enc_codec);
  codec_ctx->width = W;
  codec_ctx->height = H;
  codec_ctx->time_base = (AVRational){1, FPS};
  codec_ctx->framerate = (AVRational){FPS, 1};
  codec_ctx->pix_fmt = AV_PIX_FMT_NV12;

  if (derive_ret >= 0) {
    codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
  }

  AVDictionary *opts = NULL;
  av_dict_set(&opts, "async_depth", async_depth, 0);
  av_dict_set(&opts, "global_quality", quality, 0);

  if (avcodec_open2(codec_ctx, enc_codec, &opts) < 0) {
    fprintf(stderr, "Fatal: Could not open the QSV encoder '%s'.\n", codec_name);
    fprintf(stderr, "Verify your Intel GPU driver and that the user is in the 'video' and 'render' groups.\n");
    av_buffer_unref(&hw_device_ctx);
    return 1;
  }
  av_dict_free(&opts);
  av_buffer_unref(&hw_device_ctx);

  avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);

  avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
  (void)avformat_write_header(ofmt_ctx, NULL);

  frame = av_frame_alloc();
  frame->format = codec_ctx->pix_fmt;
  frame->width  = codec_ctx->width;
  frame->height = codec_ctx->height;
  av_frame_get_buffer(frame, 0);

  struct SwsContext *sws_ctx = sws_getContext(
      W, H, dec_ctx->pix_fmt,
      W, H, AV_PIX_FMT_NV12,
      SWS_BILINEAR, NULL, NULL, NULL);

  if (!sws_ctx) {
    fprintf(stderr, "Fatal: Could not create sws context for NV12 conversion\n");
    return 1;
  }

  pkt = av_packet_alloc();

  AVPacket *in_pkt = av_packet_alloc();
  AVFrame *dec_frame = av_frame_alloc();

  int64_t pts = 0;

  while (av_read_frame(ifmt_ctx, in_pkt) >= 0) {
    if (in_pkt->stream_index == video_stream_idx) {
      int ret = avcodec_send_packet(dec_ctx, in_pkt);
      if (ret < 0) {
        fprintf(stderr, "Error sending packet to decoder\n");
        break;
      }

      while (avcodec_receive_frame(dec_ctx, dec_frame) == 0) {
        av_frame_make_writable(frame);

        frame->pts = pts++;

        sws_scale(sws_ctx,
                  (const uint8_t **)dec_frame->data, dec_frame->linesize,
                  0, H,
                  frame->data, frame->linesize);

        avcodec_send_frame(codec_ctx, frame);
        while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
          av_packet_rescale_ts(pkt, codec_ctx->time_base, out_stream->time_base);
          pkt->stream_index = out_stream->index;
          av_interleaved_write_frame(ofmt_ctx, pkt);
          av_packet_unref(pkt);
        }
      }
    }
    av_packet_unref(in_pkt);
  }

  avcodec_send_frame(codec_ctx, NULL);
  while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
    av_packet_rescale_ts(pkt, codec_ctx->time_base, out_stream->time_base);
    pkt->stream_index = out_stream->index;
    av_interleaved_write_frame(ofmt_ctx, pkt);
    av_packet_unref(pkt);
  }

  av_frame_free(&dec_frame);
  av_packet_free(&in_pkt);
  avcodec_free_context(&dec_ctx);
  avformat_close_input(&ifmt_ctx);

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
  sws_freeContext(sws_ctx);

  double rdt = rnow - rs;
  double cdt = cnow - cs;

  printf("real time       : %.2f\n", rdt);
  printf("CPU time        : %.2f\n", cdt);
  printf("video duration  : %.2f\n", N/FPS);
  printf("temporal ratio  : %.2f\n", rdt * FPS / N);

  long compressed_size = get_file_size(out_filename);

  printf("original size   : %ldB\n", (long)H * W * 3 * N);
  printf("compressed size : %ldB\n", compressed_size);
  printf("compression ratio: %.2f\n", (double) H * W * 3 * N / compressed_size);

  return 0;
}

