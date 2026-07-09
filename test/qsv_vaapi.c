#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/dict.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>

static double get_wall_time(void) {
  struct timespec time;
  clock_gettime(CLOCK_MONOTONIC, &time);
  return (double)time.tv_sec + (double)time.tv_nsec * 1e-9;
}

static long get_file_size(const char *filename) {
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

#define DEFAULT_FPS_NUM 30
#define DEFAULT_FPS_DEN 1

static void fail_cleanup(AVFormatContext *ifmt_ctx,
                         AVFormatContext *ofmt_ctx,
                         AVCodecContext *dec_ctx,
                         AVCodecContext *enc_ctx,
                         AVBufferRef *device_ctx,
                         AVBufferRef *frames_ctx,
                         AVFrame *sw_frame,
                         AVFrame *hw_frame,
                         AVPacket *pkt,
                         AVPacket *in_pkt,
                         AVFrame *dec_frame,
                         struct SwsContext *sws_ctx) {
  sws_freeContext(sws_ctx);
  av_frame_free(&dec_frame);
  av_packet_free(&in_pkt);
  av_frame_free(&hw_frame);
  av_frame_free(&sw_frame);
  av_packet_free(&pkt);
  avcodec_free_context(&dec_ctx);
  avcodec_free_context(&enc_ctx);
  av_buffer_unref(&frames_ctx);
  av_buffer_unref(&device_ctx);
  if (ofmt_ctx) {
    if (ofmt_ctx->pb) {
      avio_closep(&ofmt_ctx->pb);
    }
    avformat_free_context(ofmt_ctx);
  }
  avformat_close_input(&ifmt_ctx);
}

int main(int argc, char **argv) {
  double rs = rnow;
  double cs = cnow;

  if (argc < 3) {
    printf("Usage: ./qsv_vaapi {input_file} {output_file}\n");
    printf("  defaults to h264_vaapi and a render-node VAAPI device at /dev/dri/renderD128\n");
    return 1;
  }

  const char *in_filename = argv[1];
  const char *out_filename = argv[2];

  AVFormatContext *ifmt_ctx = NULL;
  if (avformat_open_input(&ifmt_ctx, in_filename, NULL, NULL) < 0) {
    fprintf(stderr, "Error: Could not open input file '%s'\n", in_filename);
    return 1;
  }

  if (avformat_find_stream_info(ifmt_ctx, NULL) < 0) {
    fprintf(stderr, "Error: Could not find stream info\n");
    avformat_close_input(&ifmt_ctx);
    return 1;
  }

  int video_stream_idx = -1;
  for (unsigned int i = 0; i < ifmt_ctx->nb_streams; i++) {
    if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_idx = (int)i;
      break;
    }
  }

  if (video_stream_idx < 0) {
    fprintf(stderr, "Error: Could not find video stream in input file\n");
    avformat_close_input(&ifmt_ctx);
    return 1;
  }

  AVStream *in_stream = ifmt_ctx->streams[video_stream_idx];
  AVCodecParameters *in_codecpar = in_stream->codecpar;
  const int W = in_codecpar->width;
  const int H = in_codecpar->height;

  AVRational fps = av_guess_frame_rate(ifmt_ctx, in_stream, NULL);
  if (fps.num <= 0 || fps.den <= 0) {
    fps = (AVRational){DEFAULT_FPS_NUM, DEFAULT_FPS_DEN};
  }

  double FPS = av_q2d(fps);
  if (FPS <= 0.0 || FPS > 1000.0) {
    fps = (AVRational){DEFAULT_FPS_NUM, DEFAULT_FPS_DEN};
    FPS = av_q2d(fps);
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
    avformat_close_input(&ifmt_ctx);
    return 1;
  }

  AVCodecContext *dec_ctx = avcodec_alloc_context3(dec_codec);
  if (!dec_ctx) {
    fprintf(stderr, "Fatal: Could not allocate decoder context\n");
    avformat_close_input(&ifmt_ctx);
    return 1;
  }

  if (avcodec_parameters_to_context(dec_ctx, in_codecpar) < 0 ||
      avcodec_open2(dec_ctx, dec_codec, NULL) < 0) {
    fprintf(stderr, "Fatal: Could not open decoder\n");
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&ifmt_ctx);
    return 1;
  }

  AVFormatContext *ofmt_ctx = NULL;
  if (avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename) < 0 || !ofmt_ctx) {
    fprintf(stderr, "Fatal: Could not allocate output context\n");
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&ifmt_ctx);
    return 1;
  }

  const AVCodec *enc_codec = avcodec_find_encoder_by_name("h264_vaapi");
  if (!enc_codec) {
    fprintf(stderr, "Fatal: Could not find encoder 'h264_vaapi'\n");
    fprintf(stderr, "Did you compile FFmpeg with VAAPI support enabled?\n");
    avformat_free_context(ofmt_ctx);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&ifmt_ctx);
    return 1;
  }

  AVBufferRef *device_ctx = NULL;
  if (av_hwdevice_ctx_create(&device_ctx, AV_HWDEVICE_TYPE_VAAPI,
                             "/dev/dri/renderD128", NULL, 0) < 0) {
    fprintf(stderr, "Fatal: Could not create VAAPI device context\n");
    avformat_free_context(ofmt_ctx);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&ifmt_ctx);
    return 1;
  }

  AVBufferRef *frames_ctx = av_hwframe_ctx_alloc(device_ctx);
  if (!frames_ctx) {
    fprintf(stderr, "Fatal: Could not allocate VAAPI frames context\n");
    av_buffer_unref(&device_ctx);
    avformat_free_context(ofmt_ctx);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&ifmt_ctx);
    return 1;
  }

  AVHWFramesContext *frames = (AVHWFramesContext *)frames_ctx->data;
  frames->format = AV_PIX_FMT_VAAPI;
  frames->sw_format = AV_PIX_FMT_NV12;
  frames->width = W;
  frames->height = H;
  frames->initial_pool_size = 20;

  if (av_hwframe_ctx_init(frames_ctx) < 0) {
    fprintf(stderr, "Fatal: Could not initialize VAAPI frames context\n");
    av_buffer_unref(&frames_ctx);
    av_buffer_unref(&device_ctx);
    avformat_free_context(ofmt_ctx);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&ifmt_ctx);
    return 1;
  }

  AVStream *out_stream = avformat_new_stream(ofmt_ctx, enc_codec);
  if (!out_stream) {
    fprintf(stderr, "Fatal: Could not create output stream\n");
    av_buffer_unref(&frames_ctx);
    av_buffer_unref(&device_ctx);
    avformat_free_context(ofmt_ctx);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&ifmt_ctx);
    return 1;
  }

  AVCodecContext *enc_ctx = avcodec_alloc_context3(enc_codec);
  if (!enc_ctx) {
    fprintf(stderr, "Fatal: Could not allocate encoder context\n");
    av_buffer_unref(&frames_ctx);
    av_buffer_unref(&device_ctx);
    avformat_free_context(ofmt_ctx);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&ifmt_ctx);
    return 1;
  }

  enc_ctx->width = W;
  enc_ctx->height = H;
  enc_ctx->time_base = fps.den && fps.num ? av_inv_q(fps) : (AVRational){1, 30};
  enc_ctx->framerate = fps;
  enc_ctx->pix_fmt = AV_PIX_FMT_VAAPI;
  enc_ctx->hw_frames_ctx = av_buffer_ref(frames_ctx);
  if (!enc_ctx->hw_frames_ctx) {
    fprintf(stderr, "Fatal: Could not attach VAAPI frames context to encoder\n");
    av_buffer_unref(&frames_ctx);
    av_buffer_unref(&device_ctx);
    avcodec_free_context(&enc_ctx);
    avformat_free_context(ofmt_ctx);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&ifmt_ctx);
    return 1;
  }

  AVDictionary *opts = NULL;
  if (avcodec_open2(enc_ctx, enc_codec, &opts) < 0) {
    fprintf(stderr, "Fatal: Could not open the VAAPI encoder 'h264_vaapi'\n");
    av_dict_free(&opts);
    av_buffer_unref(&frames_ctx);
    av_buffer_unref(&device_ctx);
    avcodec_free_context(&enc_ctx);
    avformat_free_context(ofmt_ctx);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&ifmt_ctx);
    return 1;
  }
  av_dict_free(&opts);

  avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
  out_stream->time_base = enc_ctx->time_base;

  if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
    fprintf(stderr, "Fatal: Could not open output file '%s'\n", out_filename);
    fail_cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, device_ctx, frames_ctx,
                 NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    return 1;
  }

  if (avformat_write_header(ofmt_ctx, NULL) < 0) {
    fprintf(stderr, "Fatal: Could not write output header\n");
    fail_cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, device_ctx, frames_ctx,
                 NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    return 1;
  }

  AVFrame *sw_frame = av_frame_alloc();
  AVFrame *hw_frame = av_frame_alloc();
  AVPacket *pkt = av_packet_alloc();
  AVPacket *in_pkt = av_packet_alloc();
  AVFrame *dec_frame = av_frame_alloc();

  if (!sw_frame || !hw_frame || !pkt || !in_pkt || !dec_frame) {
    fprintf(stderr, "Fatal: Could not allocate working frames/packets\n");
    fail_cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, device_ctx, frames_ctx,
                 sw_frame, hw_frame, pkt, in_pkt, dec_frame, NULL);
    return 1;
  }

  sw_frame->format = AV_PIX_FMT_NV12;
  sw_frame->width = W;
  sw_frame->height = H;
  if (av_frame_get_buffer(sw_frame, 32) < 0) {
    fprintf(stderr, "Fatal: Could not allocate NV12 software frame buffer\n");
    fail_cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, device_ctx, frames_ctx,
                 sw_frame, hw_frame, pkt, in_pkt, dec_frame, NULL);
    return 1;
  }

  hw_frame->format = AV_PIX_FMT_VAAPI;
  hw_frame->width = W;
  hw_frame->height = H;
  hw_frame->hw_frames_ctx = av_buffer_ref(frames_ctx);
  if (!hw_frame->hw_frames_ctx) {
    fprintf(stderr, "Fatal: Could not attach VAAPI frames context to hardware frame\n");
    fail_cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, device_ctx, frames_ctx,
                 sw_frame, hw_frame, pkt, in_pkt, dec_frame, NULL);
    return 1;
  }

  struct SwsContext *sws_ctx = sws_getContext(
      W, H, dec_ctx->pix_fmt,
      W, H, AV_PIX_FMT_NV12,
      SWS_BILINEAR, NULL, NULL, NULL);
  if (!sws_ctx) {
    fprintf(stderr, "Fatal: Could not create sws context for NV12 conversion\n");
    fail_cleanup(ifmt_ctx, ofmt_ctx, dec_ctx, enc_ctx, device_ctx, frames_ctx,
                 sw_frame, hw_frame, pkt, in_pkt, dec_frame, NULL);
    return 1;
  }

  int64_t pts = 0;

  while (av_read_frame(ifmt_ctx, in_pkt) >= 0) {
    if (in_pkt->stream_index != video_stream_idx) {
      av_packet_unref(in_pkt);
      continue;
    }

    int ret = avcodec_send_packet(dec_ctx, in_pkt);
    av_packet_unref(in_pkt);
    if (ret < 0) {
      fprintf(stderr, "Error sending packet to decoder\n");
      break;
    }

    while ((ret = avcodec_receive_frame(dec_ctx, dec_frame)) == 0) {
      if (av_frame_make_writable(sw_frame) < 0) {
        fprintf(stderr, "Error making NV12 frame writable\n");
        break;
      }

      sws_scale(sws_ctx,
                (const uint8_t * const *)dec_frame->data, dec_frame->linesize,
                0, H,
                sw_frame->data, sw_frame->linesize);

      sw_frame->pts = pts++;

      av_frame_unref(hw_frame);
      hw_frame->format = AV_PIX_FMT_VAAPI;
      hw_frame->width = W;
      hw_frame->height = H;

      if (av_hwframe_get_buffer(frames_ctx, hw_frame, 0) < 0) {
        fprintf(stderr, "Error allocating VAAPI hardware frame\n");
        break;
      }

      if (av_hwframe_transfer_data(hw_frame, sw_frame, 0) < 0) {
        fprintf(stderr, "Error uploading frame to VAAPI\n");
        break;
      }

      ret = avcodec_send_frame(enc_ctx, hw_frame);
      if (ret < 0) {
        fprintf(stderr, "Error sending frame to encoder\n");
        break;
      }

      while (avcodec_receive_packet(enc_ctx, pkt) == 0) {
        av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);
        pkt->stream_index = out_stream->index;
        av_interleaved_write_frame(ofmt_ctx, pkt);
        av_packet_unref(pkt);
      }
    }

    if (ret == AVERROR_EOF) {
      break;
    }
  }

  avcodec_send_packet(dec_ctx, NULL);
  while (avcodec_receive_frame(dec_ctx, dec_frame) == 0) {
    if (av_frame_make_writable(sw_frame) < 0) {
      fprintf(stderr, "Error making NV12 frame writable\n");
      break;
    }

    sws_scale(sws_ctx,
              (const uint8_t * const *)dec_frame->data, dec_frame->linesize,
              0, H,
              sw_frame->data, sw_frame->linesize);

    sw_frame->pts = pts++;

    av_frame_unref(hw_frame);
    hw_frame->format = AV_PIX_FMT_VAAPI;
    hw_frame->width = W;
    hw_frame->height = H;

    if (av_hwframe_get_buffer(frames_ctx, hw_frame, 0) < 0) {
      fprintf(stderr, "Error allocating VAAPI hardware frame\n");
      break;
    }

    if (av_hwframe_transfer_data(hw_frame, sw_frame, 0) < 0) {
      fprintf(stderr, "Error uploading frame to VAAPI\n");
      break;
    }

    ret = avcodec_send_frame(enc_ctx, hw_frame);
    if (ret < 0) {
      fprintf(stderr, "Error sending frame to encoder\n");
      break;
    }

    while (avcodec_receive_packet(enc_ctx, pkt) == 0) {
      av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);
      pkt->stream_index = out_stream->index;
      av_interleaved_write_frame(ofmt_ctx, pkt);
      av_packet_unref(pkt);
    }
  }

  avcodec_send_frame(enc_ctx, NULL);
  while (avcodec_receive_packet(enc_ctx, pkt) == 0) {
    av_packet_rescale_ts(pkt, enc_ctx->time_base, out_stream->time_base);
    pkt->stream_index = out_stream->index;
    av_interleaved_write_frame(ofmt_ctx, pkt);
    av_packet_unref(pkt);
  }

  av_write_trailer(ofmt_ctx);

  av_frame_free(&dec_frame);
  av_packet_free(&in_pkt);
  av_frame_free(&hw_frame);
  av_frame_free(&sw_frame);
  av_packet_free(&pkt);
  avcodec_free_context(&dec_ctx);
  avcodec_free_context(&enc_ctx);
  av_buffer_unref(&frames_ctx);
  av_buffer_unref(&device_ctx);
  avio_closep(&ofmt_ctx->pb);
  avformat_free_context(ofmt_ctx);
  avformat_close_input(&ifmt_ctx);

  double rdt = rnow - rs;
  double cdt = cnow - cs;
  long compressed_size = get_file_size(out_filename);

  printf("real time       : %.2f\n", rdt);
  printf("CPU time        : %.2f\n", cdt);
  printf("video duration   : %.2f\n", N / FPS);
  printf("temporal ratio   : %.2f\n", rdt * FPS / N);
  printf("original size    : %ldB\n", (long)H * W * 3 * N);
  printf("compressed size  : %ldB\n", compressed_size);
  if (compressed_size > 0) {
    printf("compression ratio: %.2f\n", (double)H * W * 3 * N / compressed_size);
  }

  return 0;
}
