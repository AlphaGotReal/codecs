#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/dict.h>

#include "common.h"
#include "encoder.h"
#include "pixfmt.h"

static bool eopen(encoder_t *this) {
  avformat_alloc_output_context2(&(this->fmt_ctx), NULL, NULL, this->fname);

  const AVCodec *enc_codec = avcodec_find_encoder_by_name(this->opt->lib);
  if (!enc_codec) {
    fprintf(stderr, "Fatal: Could not find encoder '%s'.\n", this->opt->lib);
    fprintf(stderr, "Did you compile FFmpeg with this encoder enabled?\n");
    return false;
  }
  
  this->stream = avformat_new_stream(this->fmt_ctx, enc_codec);

  this->codec_ctx = avcodec_alloc_context3(enc_codec);
  this->codec_ctx->width = (int) this->opt->width;
  this->codec_ctx->height = (int) this->opt->height;
  this->codec_ctx->time_base = (AVRational){1, this->opt->fps};
  this->codec_ctx->framerate = (AVRational){this->opt->fps, 1};

  init_pixfmt();
  void *fmt = non_static_call(pixfmt, find, this->opt->fmt);
  if (fmt != NULL) {
    this->codec_ctx->pix_fmt = *(enum AVPixelFormat *)fmt;
  } else {
    fprintf(stderr, "pixel format not found '%s'\n", this->opt->fmt);
    fprintf(stderr, "defaulting to YUV420P\n");
    this->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
  }

  AVDictionary *opts = NULL;
  for (size_t t = 0; t < this->opt->n; ++t) {
    av_dict_set(&opts, this->opt->K[t], this->opt->V[t], 0);
  }

  if (avcodec_open2(this->codec_ctx, enc_codec, &opts) < 0) {
    fprintf(stderr, "could not open encoder\n");
    return false;
  }

  av_dict_free(&opts);

  avcodec_parameters_from_context(this->stream->codecpar, this->codec_ctx);
  this->stream->time_base = this->codec_ctx->time_base;

  if (avio_open(&(this->fmt_ctx->pb), this->fname, AVIO_FLAG_WRITE) < 0) {
    fprintf(stderr, "could not open output file '%s'\n", this->fname);
    return false;
  }

  if (avformat_write_header(this->fmt_ctx, NULL) < 0) {
    fprintf(stderr, "could not write header\n");
    return false;
  }

  this->frame = av_frame_alloc();
  this->frame->format = this->codec_ctx->pix_fmt;
  this->frame->width  = this->codec_ctx->width;
  this->frame->height = this->codec_ctx->height;
  av_frame_get_buffer(this->frame, 0);

  this->pkt = av_packet_alloc();
  this->pts = 0;

  return true;
}

static bool eencode(encoder_t *this, 
    const uint8_t *buf) {
  av_frame_make_writable(this->frame);

  int W = this->opt->width;
  int H = this->opt->height;
  int Y_size   = W * H;
  int UV_size  = (W / 2) * (H / 2);

  for (int y = 0; y < H; y++)
    memcpy(this->frame->data[0] + y * this->frame->linesize[0],
           buf + y * W, W);

  for (int y = 0; y < H / 2; y++)
    memcpy(this->frame->data[1] + y * this->frame->linesize[1],
           buf + Y_size + y * (W / 2), W / 2);

  for (int y = 0; y < H / 2; y++)
    memcpy(this->frame->data[2] + y * this->frame->linesize[2],
           buf + Y_size + UV_size + y * (W / 2), W / 2);

  this->frame->pts = this->pts++;

  avcodec_send_frame(this->codec_ctx, this->frame);
  while (avcodec_receive_packet(this->codec_ctx, this->pkt) == 0) {
    av_packet_rescale_ts(this->pkt, this->codec_ctx->time_base, this->stream->time_base);
    this->pkt->stream_index = this->stream->index;
    av_interleaved_write_frame(this->fmt_ctx, this->pkt);
    av_packet_unref(this->pkt);
  }

  return true;
}

static bool eflush(encoder_t *this) {
  while (avcodec_receive_packet(this->codec_ctx, this->pkt) == 0) {
    av_packet_rescale_ts(this->pkt, this->codec_ctx->time_base, this->stream->time_base);
    this->pkt->stream_index = this->stream->index;
    av_interleaved_write_frame(this->fmt_ctx, this->pkt);
    av_packet_unref(this->pkt);
  }
  return true;
}

static bool eclose(encoder_t *this) {
  avcodec_send_frame(this->codec_ctx, NULL);
  non_static_call(this, flush);

  av_write_trailer(this->fmt_ctx);

  av_frame_free(&(this->frame));
  av_packet_free(&(this->pkt));
  avcodec_free_context(&(this->codec_ctx));
  avio_closep(&(this->fmt_ctx->pb));
  avformat_free_context(this->fmt_ctx);
  return true;
}


encoder_t *new_encoder(
    const char *fname,
    encopts_t *opt) {

  encoder_t *enc = (encoder_t *) malloc(sizeof(encoder_t));
  if (enc == NULL) {
    fprintf(stderr, "failed to malloc encoder_t\n");
    return NULL;
  }

  enc->fname = fname;
  enc->opt   = opt;

  enc->fmt_ctx   = NULL;
  enc->codec_ctx = NULL;
  enc->frame     = NULL;
  enc->pkt       = NULL;
  enc->stream    = NULL;

  enc->open    = eopen; 
  enc->encode  = eencode; 
  enc->flush   = eflush; 
  enc->close   = eclose;

  return enc;  
}

void free_encoder(encoder_t *enc) {
  if (enc == NULL) return;
  if (enc->close) enc->close(enc);
  free_pixfmt();
  free(enc);
}
